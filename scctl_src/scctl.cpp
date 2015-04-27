/* Copyright (C) 2015 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

///// Songcast UPnP Controller

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/upnpputils.hxx"
#include "libupnpp/ptmutex.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/control/mediarenderer.hxx"
#include "libupnpp/control/renderingcontrol.hxx"
#include "libupnpp/control/discovery.hxx"

using namespace UPnPClient;
using namespace UPnPP;

UPnPDeviceDirectory *superdir;

MRDH getRenderer(const string& name)
{
    if (superdir == 0) {
        superdir = UPnPDeviceDirectory::getTheDir();
    }

    UPnPDeviceDesc ddesc;
    if (superdir->getDevByUDN(name, ddesc)) {
        return MRDH(new MediaRenderer(ddesc));
    } else if (superdir->getDevByFName(name, ddesc)) {
        return MRDH(new MediaRenderer(ddesc));
    } 
    cerr << "getDevByFname failed for " << name << endl;
    return MRDH();
}

struct SongcastState {
    string nm;
    string UDN;
    enum SCState {SCS_GENERROR, SCS_NOOH, SCS_NOTRECEIVER,
                  SCS_STOPPED, SCS_PLAYING};
    SCState state;
    string uri;
    string meta;
    int receiverSourceIndex;
    string reason;

    OHPRH prod;
    OHRCH rcv;

    SongcastState() 
        : state(SCS_GENERROR), receiverSourceIndex(-1) {
    }

    void reset() {
        nm.clear();
        state = SongcastState::SCS_GENERROR;
        receiverSourceIndex = -1;
        reason.clear();
        uri.clear();
        meta.clear();
        prod.reset();
        rcv.reset();
    }
};

void getSongcastState(const string& nm, SongcastState& st, bool live = true)
{
    st.reset();
    st.nm = nm;

    MRDH rdr = getRenderer(nm);
    if (!rdr) {
        st.reason = nm + " not a media renderer?";
        return;
    }
    st.nm = rdr->desc()->friendlyName;
    st.UDN = rdr->desc()->UDN;

    OHPRH prod = rdr->ohpr();
    if (!prod) {
        st.state = SongcastState::SCS_NOOH;
        st.reason =  nm + ": device has no OHProduct service";
        return;
    }
    int index;
    if (prod->sourceIndex(&index)) {
        st.reason = nm + " : sourceIndex failed";
        return;
    }

    vector<OHProduct::Source> sources;
    if (prod->getSources(sources) || sources.size() == 0) {
        st.reason = nm + ": getSources failed";
        return;
    }
    unsigned int rcvi = 0;
    for (; rcvi < sources.size(); rcvi++) {
        if (!sources[rcvi].name.compare("Receiver"))
            break;
    }
    if (rcvi == sources.size()) {
        st.state = SongcastState::SCS_NOOH;
        st.reason = nm +  " has no Receiver service";
        return;
    }
    st.receiverSourceIndex = int(rcvi);

    if (index < 0 || index >= int(sources.size())) {
        st.reason = nm +  ": bad index " + SoapHelp::i2s(index) +
            " not inside sources of size " +  SoapHelp::i2s(sources.size());
        return;
    }

    // Looks like the device has a receiver service. We may have to return a 
    // handle for it.
    OHRCH rcv = rdr->ohrc();

    string sname = sources[index].name;
    if (sname.compare("Receiver")) {
        st.state = SongcastState::SCS_NOTRECEIVER;
        st.reason = nm +  " not in receiver mode ";
        goto out;
    }

    if (!rcv) {
        st.reason = nm +  ": no receiver service??";
        goto out;
    }
    if (rcv->sender(st.uri, st.meta)) {
        st.reason = nm +  ": Receiver::Sender failed";
        goto out;
    }

    OHPlaylist::TPState tpst;
    if (rcv->transportState(&tpst)) {
        st.reason = nm +  ": Receiver::transportState() failed";
        goto out;
    }

    if (tpst == OHPlaylist::TPS_Playing) {
        st.state = SongcastState::SCS_PLAYING;
    } else {
        st.state = SongcastState::SCS_STOPPED;
    }
out:
    if (live) {
        st.prod = prod;
        st.rcv = rcv;
    }
        
    return;
}

void listReceivers(vector<SongcastState>& vscs)
{
    vector<UPnPDeviceDesc> vdds;
    if (!MediaRenderer::getDeviceDescs(vdds)) {
        cerr << "listReceivers::getDeviceDescs failed" << endl;
        return;
    }

    for (auto& entry : vdds) {
        SongcastState st;
        getSongcastState(entry.UDN, st, false);
        if (st.state == SongcastState::SCS_NOTRECEIVER || 
            st.state == SongcastState::SCS_PLAYING ||
            st.state == SongcastState::SCS_STOPPED) {
            vscs.push_back(st);
        }
    }
}

bool setReceiverPlaying(const string& nm, SongcastState& st, 
                        const string& uri, const string& meta)
{
    if (!st.rcv || !st.prod) {
        st.reason = nm + " : null handle ??";
        return false;
    }
    if (st.prod->setSourceIndex(st.receiverSourceIndex)) {
        st.reason = nm + " : can't set source index to " +
            SoapHelp::i2s(st.receiverSourceIndex);
        return false;
    }
    if (st.rcv->setSender(uri, meta)) {
        st.reason = nm + " Receiver::setSender() failed";
        return false;
    }
    if (st.rcv->play()) {
        st.reason = nm + " Receiver::play() failed";
        return false;
    }
    return true;
}

bool stopReceiver(const string& nm, SongcastState st)
{
    if (!st.rcv || !st.prod) {
        st.reason = nm + " : null handle ??";
        return false;
    }
    if (st.rcv->stop()) {
        st.reason = nm + " Receiver::play() failed";
        return false;
    }
    if (st.prod->setSourceIndex(0)) {
        st.reason = nm + " : can't set source index to " +
            SoapHelp::i2s(st.receiverSourceIndex);
        return false;
    }
    return true;
}

void ohSongcast(const string& masterName, const vector<string>& slaves)
{
    SongcastState mstate;
    getSongcastState(masterName, mstate);
    if (mstate.state != SongcastState::SCS_PLAYING) {
        cerr << "Required master not in Receiver Playing mode" << endl;
        return;
    }

    // Note: sequence sent from windows songcast when setting up a receiver:
    //   Product::SetSourceIndex / Receiver::SetSender / Receiver::Play
    // When stopping:
    //   Receiver::Stop / Product::SetStandby
    for (auto& sl: slaves) {
        cerr << "Setting up " << sl << endl;
        SongcastState sstate;
        getSongcastState(sl, sstate);

        switch (sstate.state) {
        case SongcastState::SCS_GENERROR:
        case SongcastState::SCS_NOOH:
            cerr << sl << sstate.reason << endl;
            continue;
        case SongcastState::SCS_STOPPED:
        case SongcastState::SCS_PLAYING:
            cerr << sl << ": already in receiver mode" << endl;
            continue;
        case SongcastState::SCS_NOTRECEIVER: 
            if (setReceiverPlaying(sl, sstate, mstate.uri, mstate.meta)) {
                cerr << sl << " set up for playing " << mstate.uri << endl;
            } else {
                cerr << sstate.reason << endl;
            }
        }
    }
}

void ohNoSongcast(const vector<string>& slaves)
{
    for (auto& sl: slaves) {
        cerr << "Songcast: resetting " << sl << endl;
        SongcastState sstate;
        getSongcastState(sl, sstate);

        switch (sstate.state) {
        case SongcastState::SCS_GENERROR:
        case SongcastState::SCS_NOOH:
            cerr << sl << sstate.reason << endl;
            continue;
        case SongcastState::SCS_NOTRECEIVER: 
            cerr << sl << ": not in receiver mode" << endl;
            continue;
        case SongcastState::SCS_STOPPED:
        case SongcastState::SCS_PLAYING:
            if (stopReceiver(sl, sstate)) {
                cerr << sl << " back from receiver mode " << endl;
            } else {
                cerr << sstate.reason << endl;
            }
        }
    }
}

static char *thisprog;
static char usage [] =
" -l list renderers with Songcast Receiver capability\n"
" -s <master> <slave> [slave ...] : Set up the slaves renderers as Songcast"
"    Receivers and make them play from the same uri as the master\n"
" -x <renderer> [renderer ...] Reset renderers from Songcast to Playlist\n"
"\n"
"Renderers may be designated by friendly name or UUID\n"
"  \n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}
static int   op_flags;
#define OPT_l    0x1
#define OPT_s    0x2
#define OPT_x    0x4

int main(int argc, char *argv[])
{
    thisprog = argv[0];

    int ret;
    while ((ret = getopt(argc, argv, "lsx")) != -1) {
        switch (ret) {
        case 'l': if (op_flags) Usage(); op_flags |= OPT_l; break;
        case 's': if (op_flags) Usage(); op_flags |= OPT_s; break;
        case 'x': if (op_flags) Usage(); op_flags |= OPT_x; break;
        default: Usage();
        }
    }

    if (Logger::getTheLog("/tmp/upexplo.log") == 0) {
        cerr << "Can't initialize log" << endl;
        return 1;
    }
    Logger::getTheLog("")->setLogLevel(Logger::LLDEB1);

    LibUPnP *mylib = LibUPnP::getLibUPnP();
    if (!mylib) {
        cerr << "Can't get LibUPnP" << endl;
        return 1;
    }
    if (!mylib->ok()) {
        cerr << "Lib init failed: " <<
            mylib->errAsString("main", mylib->getInitError()) << endl;
        return 1;
    }
    // mylib->setLogFileName("/tmp/libupnp.log");


    if ((op_flags & OPT_l)) {
        if (op_flags & (OPT_s|OPT_x)) {
            Usage();
        }
        if (optind < argc)
            Usage();
        vector<SongcastState> vscs;
        listReceivers(vscs);
        for (auto& scs: vscs) {
            switch (scs.state) {
            case SongcastState::SCS_GENERROR:    cout << "Error ";break;
            case SongcastState::SCS_NOOH:        cout << "Nooh  ";break;
            case SongcastState::SCS_NOTRECEIVER: cout << "Off   ";break;
            case SongcastState::SCS_STOPPED:     cout << "Stop  ";break;
            case SongcastState::SCS_PLAYING:     cout << "Play  ";break;
            }
            cout << scs.nm << " ";
            cout << scs.UDN << " ";
            if (scs.state == SongcastState::SCS_PLAYING) {
                cout << scs.uri;
            } else if (scs.state == SongcastState::SCS_GENERROR) {
                cout << scs.reason;
            }
            cout << endl;
        }
        
    } else if ((op_flags & OPT_s)) {
        if (op_flags & (OPT_l|OPT_x)) {
            Usage();
        }
        if (optind > argc - 2)
            Usage();
        string master = argv[optind++];
        vector<string> slaves;
        while (optind < argc) {
            slaves.push_back(argv[optind++]);
        }
        ohSongcast(master, slaves);
    } else if ((op_flags & OPT_x)) {
        if (op_flags & (OPT_l|OPT_s)) {
            Usage();
        }
        if (optind > argc - 1)
            Usage();
        vector<string> slaves;
        while (optind < argc) {
            slaves.push_back(argv[optind++]);
        }
        ohNoSongcast(slaves);
    } else {
        Usage();
    }

    return 0;
}
