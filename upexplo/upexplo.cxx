/* Copyright (C) 2013 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

////////////////////// libupnpp UPnP explorer test program

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <vector>
using namespace std;

#include "libupnpp/log.hxx"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/discovery.hxx"
#include "libupnpp/description.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/control/cdirectory.hxx"
#include "libupnpp/control/mediarenderer.hxx"
#include "libupnpp/control/renderingcontrol.hxx"

using namespace UPnPClient;

void listServers()
{
    cout << "Content Directories:" << endl;
    vector<CDSH> dirservices;
    if (!ContentDirectoryService::getServices(dirservices)) {
        cerr << "listDirServices failed" << endl;
        return;
    }
    for (vector<CDSH>::iterator it = dirservices.begin();
         it != dirservices.end(); it++) {
        cout << (*it)->getFriendlyName() << endl;
    }
    cout << endl;
}

void listPlayers()
{
    cout << "Media Renderers:" << endl;
    vector<UPnPDeviceDesc> vdds;
    if (!MediaRenderer::getDeviceDescs(vdds)) {
        cerr << "MediaRenderer::getDeviceDescs" << endl;
        return;
    }
    for (auto& entry : vdds) {
        cout << entry.friendlyName << endl;
    }
    cout << endl;
}

class MReporter : public UPnPClient::VarEventReporter {
public:
    void changed(const char *nm, int value)
        {
            cout << "Changed: " << nm << " : " << value << endl;
        }
    void changed(const char *nm, const char *value)
        {
            cout << "Changed: " << nm << " : " << value << endl;
        }

    void changed(const char *nm, UPnPDirContent meta)
        {
            string s("NO CONTENT");
            if (meta.m_items.size() > 0) {
                s = meta.m_items[0].dump();
            }
            cout << "Changed: " << nm << " : " << s << endl;
        }

};

MRDH getRenderer(const string& friendlyName)
{
    MRDH rdr;
    vector<UPnPDeviceDesc> vdds;
    if (!MediaRenderer::getDeviceDescs(vdds, friendlyName)) {
        cerr << "MediaRenderer::getDeviceDescs" << endl;
        return rdr;
    }

    if (vdds.size() == 0) {
        cerr << "Player not found" << endl;
        return rdr;
    } else if (vdds.size() > 1) {
        cerr << "Multiple players found" << endl;
        return rdr;
    }

    UPnPDeviceDesc& dev = vdds[0];
    return MRDH(new MediaRenderer(dev));
}

void getsetVolume(const string& friendlyName, int volume = -1)
{
    MRDH rdr = getRenderer(friendlyName);
    if (!rdr) {
        return;
    }

    RDCH rdc = rdr->rdc();
    if (!rdc) {
        cerr << "Device has no RenderingControl service" << endl;
        return;
    }

    MReporter reporter;
    rdc->installReporter(&reporter);

    if (volume == -1) {
        volume = rdc->getVolume();
        cout << "Current volume: " << volume << " reporter " << 
            rdc->getReporter() << endl;
#warning remove
        while (true)
            sleep(20);
        return;
    } else {
        if ((volume = rdc->setVolume(volume)) != 0) {
            cerr << "Error setting volume: " << volume << endl;
            return;
        }
    }
}

void tpPlayStop(const string& friendlyName, bool doplay)
{
    MRDH rdr = getRenderer(friendlyName);
    if (!rdr) {
        return;
    }
    AVTH avt = rdr->avt();
    if (!avt) {
        cerr << "Device has no AVTransport service" << endl;
        return;
    }
    MReporter reporter;
    avt->installReporter(&reporter);
    int ret;
    if (doplay) {
        ret = avt->play();
    } else {
        ret = avt->stop();
    }
    if (ret != 0) {
        cerr << "Operation failed: code: " << ret << endl;
    }
#warning remove
    while (true) {
        AVTransport::PositionInfo info;
        if (avt->getPositionInfo(info)) {
            cerr << "getPositionInfo failed. Code " << ret << endl;
        } else {
            cout << info.trackmeta.m_title << " reltime " << info.reltime << endl;
        }
        sleep(2);
    }
}

void readdir(LibUPnP *lib, const string& friendlyName, const string& cid)
{
    cout << "readdir: [" << friendlyName << "] [" << cid << "]" << endl;
    CDSH server;
    if (!ContentDirectoryService::getServerByName(friendlyName, server)) {
        cerr << "Server not found" << endl;
        return;
    }
    UPnPDirContent dirbuf;
    int code = server->readDir(cid, dirbuf);
    if (code) {
        cerr << LibUPnP::errAsString("readdir", code) << endl;
        return;
    }
    cout << "Browse: got " << dirbuf.m_containers.size() <<
        " containers and " << dirbuf.m_items.size() << " items " << endl;
    for (unsigned int i = 0; i < dirbuf.m_containers.size(); i++) {
        cout << dirbuf.m_containers[i].dump();
    }
    for (unsigned int i = 0; i < dirbuf.m_items.size(); i++) {
        cout << dirbuf.m_items[i].dump();
    }
}

void getMetadata(LibUPnP *lib, const string& friendlyName, const string& cid)
{
    cout << "getMeta: [" << friendlyName << "] [" << cid << "]" << endl;
    CDSH server;
    if (!ContentDirectoryService::getServerByName(friendlyName, server)) {
        cerr << "Server not found" << endl;
        return;
    }
    UPnPDirContent dirbuf;
    int code = server->getMetadata(cid, dirbuf);
    if (code) {
        cerr << LibUPnP::errAsString("readdir", code) << endl;
        return;
    }
    cout << "getMeta: got " << dirbuf.m_containers.size() <<
        " containers and " << dirbuf.m_items.size() << " items " << endl;
    for (unsigned int i = 0; i < dirbuf.m_containers.size(); i++) {
        cout << dirbuf.m_containers[i].dump();
    }
    for (unsigned int i = 0; i < dirbuf.m_items.size(); i++) {
        cout << dirbuf.m_items[i].dump();
    }
}

void search(LibUPnP *lib, const string& friendlyName, const string& ss)
{
    cout << "search: [" << friendlyName << "] [" << ss << "]" << endl;
    CDSH server;
    if (!ContentDirectoryService::getServerByName(friendlyName, server)) {
        cerr << "Server not found" << endl;
        return;
    }
    UPnPDirContent dirbuf;
    string cid("0");
    int code = server->search(cid, ss, dirbuf);
    if (code) {
        cerr << LibUPnP::errAsString("search", code) << endl;
        return;
    }
    cout << "Search: got " << dirbuf.m_containers.size() <<
        " containers and " << dirbuf.m_items.size() << " items " << endl;
    for (unsigned int i = 0; i < dirbuf.m_containers.size(); i++) {
        cout << dirbuf.m_containers[i].dump();
    }
    for (unsigned int i = 0; i < dirbuf.m_items.size(); i++) {
        cout << dirbuf.m_items[i].dump();
    }
}

void getSearchCaps(LibUPnP *lib, const string& friendlyName)
{
    cout << "getSearchCaps: [" << friendlyName << "]" << endl;
    CDSH server;
    if (!ContentDirectoryService::getServerByName(friendlyName, server)) {
        cerr << "Server not found" << endl;
        return;
    }
    set<string> capa;
    int code = server->getSearchCapabilities(capa);
    if (code) {
        cerr << LibUPnP::errAsString("readdir", code) << endl;
        return;
    }
    if (capa.empty()) {
        cout << "No search capabilities";
    } else {
        for (set<string>::const_iterator it = capa.begin();
             it != capa.end(); it++) {
            cout << "[" << *it << "]";
        }
    }
    cout << endl;
}

static char *thisprog;
static char usage [] =
            " -l : list servers\n"
            " -r <server> <objid> list object id (root is '0')\n"
            " -s <server> <searchstring> search for string\n"
            " -m <server> <objid> : list object metadata\n"
            " -c <server> get search capabilities\n"
            " -v <renderer> get volume\n"
            " -V <renderer> <volume> set volume\n"
            " -p <renderer> 1|0 play/stop\n"
            "  \n\n"
            ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}
static int	   op_flags;
#define OPT_MOINS 0x1
#define OPT_l	  0x2
#define OPT_r	  0x4
#define OPT_c	  0x8
#define OPT_s	  0x10
#define OPT_m	  0x20
#define OPT_v	  0x40
#define OPT_V	  0x80
#define OPT_p	  0x100

int main(int argc, char *argv[])
{
    string fname;
    string arg;
    int volume = -1;
    int iarg = 0;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
                fname = *(++argv);argc--;
                goto b1;
            case 'l':	op_flags |= OPT_l; break;
            case 'm':	op_flags |= OPT_m; if (argc < 3)  Usage();
                fname = *(++argv);argc--;
                arg = *(++argv);argc--;
                goto b1;
            case 'p':	op_flags |= OPT_p; if (argc < 3)  Usage();
                fname = *(++argv);argc--;
                iarg = atoi(*(++argv)); argc--;
                goto b1;
            case 'r':	op_flags |= OPT_r; if (argc < 3)  Usage();
                fname = *(++argv);argc--;
                arg = *(++argv);argc--;
                goto b1;
            case 's':	op_flags |= OPT_s; if (argc < 3)  Usage();
                fname = *(++argv);argc--;
                arg = *(++argv);argc--;
                goto b1;
            case 'V':	op_flags |= OPT_V; if (argc < 3)  Usage();
                fname = *(++argv);argc--;
                volume = atoi(*(++argv));argc--;
                goto b1;
            case 'v':	op_flags |= OPT_v; if (argc < 2)  Usage();
                fname = *(++argv);argc--;
                goto b1;

            default: Usage();	break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0)
        Usage();

    if (upnppdebug::Logger::getTheLog("/tmp/upexplo.log") == 0) {
        cerr << "Can't initialize log" << endl;
        return 1;
    }

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
    mylib->setLogFileName("/tmp/libupnp.log");
    UPnPDeviceDirectory *superdir = UPnPDeviceDirectory::getTheDir();
    if (!superdir || !superdir->ok()) {
        cerr << "Discovery services startup failed" << endl;
        return 1;
    }

    if ((op_flags & OPT_l)) {
        while (true) {
            listServers();
            listPlayers();
            sleep(5);
        }
    } else if ((op_flags & OPT_m)) {
        getMetadata(mylib, fname, arg);
    } else if ((op_flags & OPT_r)) {
        readdir(mylib, fname, arg);
    } else if ((op_flags & OPT_s)) {
        search(mylib, fname, arg);
    } else if ((op_flags & OPT_c)) {
        getSearchCaps(mylib, fname);
    } else if ((op_flags & OPT_V)) {
        getsetVolume(fname, volume);
    } else if ((op_flags & OPT_v)) {
        getsetVolume(fname);
    } else if ((op_flags & OPT_p)) {
        tpPlayStop(fname, iarg);
    } else {
        Usage();
    }
    superdir->terminate();
    return 0;
}
