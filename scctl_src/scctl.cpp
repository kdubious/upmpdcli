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

/* 
 * Songcast UPnP controller. 
 *
 * This can list the state of all Receivers, tell
 * a Receiver to play the same URI as another one (master/slave,
 * except that the slaves don't really follow the master state, they
 * will just keep playing the same URI), or tell a Receiver to return
 * to Playlist operation.
 * 
 * To avoid encurring a discovery timeout for each op, there is a
 * server mode, in which a permanent process executes the above
 * commands, received on Unix socket, and returns the results.
 *
 * When executing any of the ops from the command line, the program
 * first tries to contact the server, and does things itself if no
 * server is found (encurring 2-3 S of timeout in the latter case).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/control/mediarenderer.hxx"
#include "libupnpp/control/discovery.hxx"

#include "../src/netcon.h"
#include "../src/upmpdutils.hxx"

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

string showReceivers()
{
    vector<SongcastState> vscs;
    listReceivers(vscs);
    ostringstream out;

    for (auto& scs: vscs) {
        switch (scs.state) {
        case SongcastState::SCS_GENERROR:    out << "Error ";break;
        case SongcastState::SCS_NOOH:        out << "Nooh  ";break;
        case SongcastState::SCS_NOTRECEIVER: out << "Off   ";break;
        case SongcastState::SCS_STOPPED:     out << "Stop  ";break;
        case SongcastState::SCS_PLAYING:     out << "Play  ";break;
        }
        out << scs.nm << " ";
        out << scs.UDN << " ";
        if (scs.state == SongcastState::SCS_PLAYING) {
            out << scs.uri;
        } else if (scs.state == SongcastState::SCS_GENERROR) {
            out << scs.reason;
        }
        out << endl;
    }
    return out.str();
}

static char *thisprog;
static char usage [] =
" -l List renderers with Songcast Receiver capability\n"
" -s <master> <slave> [slave ...] : Set up the slaves renderers as Songcast\n"
"    Receivers and make them play from the same uri as the master\n"
" -x <renderer> [renderer ...] Reset renderers from Songcast to Playlist\n"
" -S Run as server\n"
" -f If no server is found, scctl will fork one after performing the\n"
"    requested command, so that the next execution will not have to wait for\n"
"    the discovery timeout.n"
" -h This help.\n"
"\n"
"Renderers may be designated by friendly name or UUID\n"
"\n"
;
static void
Usage(FILE *fp = stderr)
{
    fprintf(fp, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int   op_flags;
#define OPT_l    0x1
#define OPT_s    0x2
#define OPT_x    0x4
#define OPT_S    0x8
#define OPT_h    0x10
#define OPT_f    0x20
#define OPT_p    0x40

int runserver();
bool tryserver(int flags, int argc, char *argv[]);

int main(int argc, char *argv[])
{
    thisprog = argv[0];

    int ret;
    while ((ret = getopt(argc, argv, "fhlsSx")) != -1) {
        switch (ret) {
        case 'f': op_flags |= OPT_f; break;
        case 'h': Usage(stdout); break;
        case 'l': op_flags |= OPT_l; break;
        case 's': op_flags |= OPT_s; break;
        case 'S': op_flags |= OPT_S; break;
        case 'x': op_flags |= OPT_x; break;
        default: Usage();
        }
    }
    //fprintf(stderr, "argc %d optind %d flgs: 0x%x\n", argc, optind, op_flags);

    // If we're not a server, try to contact one to avoid the
    // discovery timeout
    if (!(op_flags & OPT_S) && tryserver(op_flags, argc -optind, 
                                         &argv[optind])) {
        exit(0);
    }

    if ((op_flags & ~OPT_f) == 0)
        Usage();

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
        string out = showReceivers();
        cout << out;
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
    } else if ((op_flags & OPT_S)) {
        exit(runserver());
    } else {
        Usage();
    }

    // If we get here, we have executed a local command. If -f is set,
    // fork and run the server code so that a next execution will use
    // this instead (and not incur the discovery timeout)
    if ((op_flags & OPT_f)) {
        // Father exits, son process becomes server
        if (daemon(0, 0) == 0)
            runserver();
    } 
    return 0;
}

// The Unix socket path which we use for client-server operation
bool sockname(string& nm)
{
    char buf[80];
    sprintf(buf, "/tmp/scctl%d", int(getuid()));
    if (access(buf, 0) < 0) {
        if (mkdir(buf, 0700)) {
            perror("mkdir");
            return false;
        }
    }
    sprintf(buf, "/tmp/scctl%d/sock", int(getuid()));
    nm = buf;
    return true;
}


// Try to have an op run in server process (to avoid the discovery
// timeout).
bool tryserver(const string& cmd)
{
    fflush(stdout);

    NetconCli *clicon = new NetconCli();
    NetconP con(clicon);
    if (!con) {
        cerr << "tryserver: new NetconCli failed\n";
        return false;
    }
    
    string snm;
    if (!sockname(snm)) {
        return false;
    }
    if (clicon->openconn(snm.c_str(), (unsigned int)0) < 0) {
        // Server not running case, no big deal
        LOGDEB("openconn(" << snm << ") failed (ok: server not running)\n");
        return false;
    }

    if (clicon->send(cmd.c_str(), cmd.size() + 1) < 0) {
        cerr << "Send failed\n";
        return false;
    }
    char buf[1024];
    for (;;) {
        int cnt = clicon->receive(buf, 1024);
        if (cnt < 0) {
            perror("receive:");
            return false;
        }
        if (cnt == 0)
            break;
        if (write(1, buf, cnt) != cnt) {
            perror("write");
            return false;
        }
    }
    return true;
}

bool tryserver(int opflags, int argc, char **argv)
{
    char opts[30];
    sprintf(opts, "0x%x", opflags);
    string cmd(opts);
    cmd += " ";
        
    for (int i = 0; i < argc; i++) {
        // May need quoting here ? 
        cmd  += argv[i]; 
        cmd += " ";
    }
    cmd += "\n";
    return tryserver(cmd);
}


// Listening endpoint for the server. For each connection, one request
// is served immediately and the connection is closed.
class MyNetconServLis : public NetconServLis {
public:
protected:
    int cando(Netcon::Event reason);
};

// Server worker method. Called for each connection, does one thing
// and returns.
int MyNetconServLis::cando(Netcon::Event reason)
{
    NetconServCon *con = accept();
    if (con == 0) {
        LOGERR("scctl server: accept() failed\n");
        return 1;
    }
    std::unique_ptr<Netcon> conhold(con);

    // Get command
    string line;
    {
        char buf[2048];
        if  (con->getline(buf, 2048, 2) <= 0) {
            LOGERR("scctl: server: getline() failed\n");
            return 1;
        }
        line = buf;
    }

    trimstring(line, " \n");

    LOGDEB1("scctl: server: got cmd: " << line << endl);

    vector<string> toks;
    stringToTokens(line, toks);
    if (toks.empty()) {
        return 1;
    }

    int opflags = strtoul(toks[0].c_str(), 0, 0);

    string out;
    if (opflags & OPT_p) {
        // ping
        out = "Ok\n";
    } else if (opflags & OPT_l) {
        out = showReceivers();
    } else if (opflags & OPT_s) {
        if (toks.size() < 3)
            return 1;
        vector<string>::iterator beg = toks.begin();
        beg++;
        string master = *beg;
        beg++;
        vector<string> slaves(beg, toks.end());
        ohSongcast(master, slaves);
    } else if (opflags & OPT_x) {
        if (toks.size() < 2)
            return 1;
        vector<string>::iterator beg = toks.begin();
        beg++;
        vector<string> slaves(beg, toks.end());
        ohNoSongcast(slaves);
    } else {
        LOGERR("scctl: server: bad cmd:" << toks[0] << endl);
        return 1;
    }

    if (con->send(out.c_str(), out.size(), 0) < 0) {
        LOGERR("scctl: server: send() failed\n");
        return 1;
    }

    return 1;
}

// Server init routine

int runserver()
{
    string snm;
    if (!sockname(snm)) {
        return 1;
    }

    // Check if already running, else clean up socket
    if (access(snm.c_str(), 0) == 0) {
        
        if(tryserver("-p\n")) {
            // Already running
            return 0;
        }
        if (unlink(snm.c_str()) < 0) {
            perror("unlink");
            return 1;
        }
    }

    // Run
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    // Initialize lib at once, will be ready when we need it
    showReceivers();

    MyNetconServLis *servlis = new MyNetconServLis();
    if (servlis == 0) {
        LOGERR("scctl: server: new NetconServLis failed\n");
        return 1;
    }
    if (servlis->openservice(snm.c_str()) < 0) {
        LOGERR("scctl: server: openservice(" << snm << ") failed\n");
        return 1;
    }

    SelectLoop myloop;
    myloop.addselcon(NetconP(servlis), Netcon::NETCONPOLL_READ);

    LOGDEB("scctl: server: openservice(" << snm << ") Ok\n");

    if (myloop.doLoop() < 0) {
        LOGERR("scctl: server: selectloop failed\n");
        return 1;
    }
    return 0;
}
