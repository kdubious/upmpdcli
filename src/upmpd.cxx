/* Copyright (C) 2014 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "upmpd.hxx"

#include <errno.h>                      // for errno
#include <fcntl.h>                      // for open, O_CREAT, O_RDWR
#include <pwd.h>                        // for getpwnam, passwd
#include <signal.h>                     // for sigaction, SIG_IGN, etc
#include <stdio.h>                      // for fprintf, perror, stderr
#include <stdlib.h>                     // for atoi, getenv, exit
#include <sys/param.h>                  // for MIN
#include <unistd.h>                     // for geteuid, chown, sleep, etc
#include <string.h>                     // for memset
#include <grp.h>

#include <iostream>                     // for basic_ostream, operator<<, etc
#include <string>                       // for string, operator<<, etc
#include <unordered_map>                // for unordered_map, etc
#include <vector>                       // for vector, vector<>::iterator

#include "libupnpp/device/device.hxx"   // for UpnpDevice, UpnpService
#include "libupnpp/log.hxx"             // for LOGFAT, LOGERR, Logger, etc
#include "libupnpp/upnpplib.hxx"        // for LibUPnP

#include "avtransport.hxx"
#include "conman.hxx"
#include "mpdcli.hxx"
#include "ohinfo.hxx"
#include "ohplaylist.hxx"
#include "ohradio.hxx"
#include "ohproduct.hxx"
#include "ohreceiver.hxx"
#include "ohtime.hxx"
#include "ohvolume.hxx"
#include "renderctl.hxx"
#include "upmpdutils.hxx"
#include "execmd.h"
#include "httpfs.hxx"
#include "ohsndrcv.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

// Note: if we ever need this to work without cxx11, there is this:
// http://www.tutok.sk/fastgl/callback.html
UpMpd::UpMpd(const string& deviceid, const string& friendlyname,
             const unordered_map<string, VDirContent>& files,
             MPDCli *mpdcli, Options opts)
    : UpnpDevice(deviceid, files), m_mpdcli(mpdcli), m_mpds(0),
      m_options(opts.options),
      m_mcachefn(opts.cachefn),
      m_rdctl(0), m_avt(0), m_ohpr(0), m_ohpl(0), m_ohrcv(0), m_sndrcv(0),
      m_friendlyname(friendlyname)
{
    bool avtnoev = (m_options & upmpdNoAV) != 0; 
    // Note: the order is significant here as it will be used when
    // calling the getStatus() methods, and we want AVTransport to
    // update the mpd status for OHInfo
    m_rdctl = new UpMpdRenderCtl(this, avtnoev);
    m_services.push_back(m_rdctl);
    m_avt = new UpMpdAVTransport(this, avtnoev);
    m_services.push_back(m_avt);
    m_services.push_back(new UpMpdConMan(this));

    if (m_options & upmpdDoOH) {
        m_services.push_back(new OHInfo(this));
        m_services.push_back(new OHTime(this));
        m_services.push_back(new OHVolume(this));
        m_ohpl = new OHPlaylist(this, opts.ohmetasleep);
        m_services.push_back(m_ohpl);
        m_ohrd = new OHRadio(this);
        m_services.push_back(m_ohrd);
        if (m_options & upmpdOhReceiver) {
            struct OHReceiverParams parms;
            if (opts.schttpport)
                parms.httpport = opts.schttpport;
            if (!opts.scplaymethod.empty()) {
                if (!opts.scplaymethod.compare("alsa")) {
                    parms.pm = OHReceiverParams::OHRP_ALSA;
                } else if (!opts.scplaymethod.compare("mpd")) {
                    parms.pm = OHReceiverParams::OHRP_MPD;
                }
            }
            parms.sc2mpdpath = opts.sc2mpdpath;
            m_ohrcv = new OHReceiver(this, parms);
            m_services.push_back(m_ohrcv);
        }
        if (m_options& upmpdOhSenderReceiver) {
            // Note: this is not an UPnP service
            m_sndrcv = new SenderReceiver(this, opts.senderpath,
                                          opts.sendermpdport);
        }
        // Create ohpr last, so that it can ask questions to other services
        m_ohpr = new OHProduct(this, friendlyname);
        m_services.push_back(m_ohpr);
    }
}

UpMpd::~UpMpd()
{
    delete m_sndrcv;
    for (vector<UpnpService*>::iterator it = m_services.begin();
         it != m_services.end(); it++) {
        delete(*it);
    }
}

const MpdStatus& UpMpd::getMpdStatus()
{
    m_mpds = &m_mpdcli->getStatus();
    return *m_mpds;
}

/////////////////////////////////////////////////////////////////////
// Main program

#include "conftree.hxx"

static char *thisprog;

static int op_flags;
#define OPT_MOINS 0x1
#define OPT_h     0x2
#define OPT_p     0x4
#define OPT_d     0x8
#define OPT_D     0x10
#define OPT_c     0x20
#define OPT_l     0x40
#define OPT_f     0x80
#define OPT_q     0x100
#define OPT_i     0x200
#define OPT_P     0x400
#define OPT_O     0x800

static const char usage[] = 
    "-c configfile \t configuration file to use\n"
    "-h host    \t specify host MPD is running on\n"
    "-p port     \t specify MPD port\n"
    "-d logfilename\t debug messages to\n"
    "-l loglevel\t  log level (0-6)\n"
    "-D    \t run as a daemon\n"
    "-f friendlyname\t define device displayed name\n"
    "-q 0|1\t if set, we own the mpd queue, else avoid clearing it whenever we feel like it\n"
    "-i iface    \t specify network interface name to be used for UPnP\n"
    "-P upport    \t specify port number to be used for UPnP\n"
    "-O 0|1\t decide if we run and export the OpenHome services\n"
    "\n"
    ;

static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static const string dfltFriendlyName("UpMpd");

// This is global
string g_protocolInfo;

// Static for cleanup in sig handler.
static UpnpDevice *dev;

string g_datadir(DATADIR "/");

// Global
string g_configfilename(CONFIGDIR "/upmpdcli.conf");
ConfSimple *g_config;

static void onsig(int)
{
    LOGDEB("Got sig" << endl);
    dev->shouldExit();
}

static const int catchedSigs[] = {SIGINT, SIGQUIT, SIGTERM};
static void setupsigs()
{
    struct sigaction action;
    action.sa_handler = onsig;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++)
        if (signal(catchedSigs[i], SIG_IGN) != SIG_IGN) {
            if (sigaction(catchedSigs[i], &action, 0) < 0) {
                perror("Sigaction failed");
            }
        }
}

int main(int argc, char *argv[])
{
    // Path for the sc2mpd command, or empty
    string sc2mpdpath;

    // Sender mode: path for the command creating the mpd and mpd2sc
    // processes, and port for the auxiliary mpd.
    string senderpath;
    int sendermpdport = 6700;

    // Main MPD parameters
    string mpdhost("localhost");
    int mpdport = 6600;
    string mpdpassword;

    string logfilename;
    int loglevel(Logger::LLINF);
    string friendlyname(dfltFriendlyName);
    bool ownqueue = true;
    bool enableAV = true;
    bool enableOH = true;
    bool ohmetapersist = true;
    string upmpdcliuser("upmpdcli");
    string pidfilename("/var/run/upmpdcli.pid");
    string iconpath(DATADIR "/icon.png");
    string presentationhtml(DATADIR "/presentation.html");
    string iface;
    unsigned short upport = 0;
    string upnpip;

    const char *cp;
    if ((cp = getenv("UPMPD_HOST")))
        mpdhost = cp;
    if ((cp = getenv("UPMPD_PORT")))
        mpdport = atoi(cp);
    if ((cp = getenv("UPMPD_FRIENDLYNAME")))
        friendlyname = atoi(cp);
    if ((cp = getenv("UPMPD_CONFIG")))
        g_configfilename = cp;
    if ((cp = getenv("UPMPD_UPNPIFACE")))
        iface = cp;
    if ((cp = getenv("UPMPD_UPNPPORT")))
        upport = atoi(cp);

    thisprog = argv[0];
    argc--; argv++;
    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'c':   op_flags |= OPT_c; if (argc < 2)  Usage();
                g_configfilename = *(++argv); argc--; goto b1;
            case 'D':   op_flags |= OPT_D; break;
            case 'd':   op_flags |= OPT_d; if (argc < 2)  Usage();
                logfilename = *(++argv); argc--; goto b1;
            case 'f':   op_flags |= OPT_f; if (argc < 2)  Usage();
                friendlyname = *(++argv); argc--; goto b1;
            case 'h':   op_flags |= OPT_h; if (argc < 2)  Usage();
                mpdhost = *(++argv); argc--; goto b1;
            case 'i':   op_flags |= OPT_i; if (argc < 2)  Usage();
                iface = *(++argv); argc--; goto b1;
            case 'l':   op_flags |= OPT_l; if (argc < 2)  Usage();
                loglevel = atoi(*(++argv)); argc--; goto b1;
            case 'O': {
                op_flags |= OPT_O; 
                if (argc < 2)  Usage();
                const char *cp =  *(++argv);
                if (*cp == '1' || *cp == 't' || *cp == 'T' || *cp == 'y' || 
                    *cp == 'Y')
                    enableOH = true;
                argc--; goto b1;
            }
            case 'P':   op_flags |= OPT_P; if (argc < 2)  Usage();
                upport = atoi(*(++argv)); argc--; goto b1;
            case 'p':   op_flags |= OPT_p; if (argc < 2)  Usage();
                mpdport = atoi(*(++argv)); argc--; goto b1;
            case 'q':   op_flags |= OPT_q; if (argc < 2)  Usage();
                ownqueue = atoi(*(++argv)) != 0; argc--; goto b1;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0)
        Usage();

    UpMpd::Options opts;

    string cachedir;
    string onstart;
    string onstop;
    string onvolumechange;
    if (!g_configfilename.empty()) {
        g_config = new ConfSimple(g_configfilename.c_str(), 1, true);
        if (!g_config || !g_config->ok()) {
            cerr << "Could not open config: " << g_configfilename << endl;
            return 1;
        }

        string value;
        if (!(op_flags & OPT_d))
            g_config->get("logfilename", logfilename);
        if (!(op_flags & OPT_f))
            g_config->get("friendlyname", friendlyname);
        if (!(op_flags & OPT_l) && g_config->get("loglevel", value))
            loglevel = atoi(value.c_str());
        if (!(op_flags & OPT_h))
            g_config->get("mpdhost", mpdhost);
        if (!(op_flags & OPT_p) && g_config->get("mpdport", value)) {
            mpdport = atoi(value.c_str());
        }
        g_config->get("mpdpassword", mpdpassword);
        if (!(op_flags & OPT_q) && g_config->get("ownqueue", value)) {
            ownqueue = atoi(value.c_str()) != 0;
        }
        if (g_config->get("openhome", value)) {
            enableOH = atoi(value.c_str()) != 0;
        }
        if (g_config->get("upnpav", value)) {
            enableAV = atoi(value.c_str()) != 0;
        }
        if (g_config->get("ohmetapersist", value)) {
            ohmetapersist = atoi(value.c_str()) != 0;
        }
        g_config->get("iconpath", iconpath);
        g_config->get("presentationhtml", presentationhtml);
        g_config->get("cachedir", cachedir);
        g_config->get("onstart", onstart);
        g_config->get("onstop", onstop);
        g_config->get("onvolumechange", onvolumechange);
        if (!(op_flags & OPT_i)) {
            g_config->get("upnpiface", iface);
            if (iface.empty()) {
                g_config->get("upnpip", upnpip);
            }
        }
        if (!(op_flags & OPT_P) && g_config->get("upnpport", value)) {
            upport = atoi(value.c_str());
        }
        if (g_config->get("schttpport", value))
            opts.schttpport = atoi(value.c_str());
        g_config->get("scplaymethod", opts.scplaymethod);
        g_config->get("sc2mpd", sc2mpdpath);
        if (g_config->get("ohmetasleep", value))
            opts.ohmetasleep = atoi(value.c_str());

        g_config->get("scsenderpath", senderpath);
        if (g_config->get("scsendermpdport", value))
            sendermpdport = atoi(value.c_str());
    }
    if (Logger::getTheLog(logfilename) == 0) {
        cerr << "Can't initialize log" << endl;
        return 1;
    }
    Logger::getTheLog("")->setLogLevel(Logger::LogLevel(loglevel));

    Pidfile pidfile(pidfilename);

    // If started by root, do the pidfile + change uid thing
    uid_t runas(0);
    gid_t runasg(0);
    if (geteuid() == 0) {
        struct passwd *pass = getpwnam(upmpdcliuser.c_str());
        if (pass == 0) {
            LOGFAT("upmpdcli won't run as root and user " << upmpdcliuser << 
                   " does not exist " << endl);
            return 1;
        }
        runas = pass->pw_uid;
        runasg = pass->pw_gid;

        pid_t pid;
        if ((pid = pidfile.open()) != 0) {
            LOGFAT("Can't open pidfile: " << pidfile.getreason() << 
                   ". Return (other pid?): " << pid << endl);
            return 1;
        }
        if (pidfile.write_pid() != 0) {
            LOGFAT("Can't write pidfile: " << pidfile.getreason() << endl);
            return 1;
        }
	if (cachedir.empty())
            cachedir = "/var/cache/upmpdcli";
    } else {
	if (cachedir.empty())
            cachedir = path_cat(path_tildexpand("~") , "/.cache/upmpdcli");
    }

    string& mcfn = opts.cachefn;
    if (ohmetapersist) {
        opts.cachefn = path_cat(cachedir, "/metacache");
        if (!path_makepath(cachedir, 0755)) {
            LOGERR("makepath("<< cachedir << ") : errno : " << errno << endl);
        } else {
            int fd;
            if ((fd = open(mcfn.c_str(), O_CREAT|O_RDWR, 0644)) < 0) {
                LOGERR("creat("<< mcfn << ") : errno : " << errno << endl);
            } else {
                close(fd);
                if (geteuid() == 0 && chown(mcfn.c_str(), runas, -1) != 0) {
                    LOGERR("chown("<< mcfn << ") : errno : " << errno << endl);
                }
                if (geteuid() == 0 && chown(cachedir.c_str(), runas, -1) != 0) {
                    LOGERR("chown("<< cachedir << ") : errno : " << errno << endl);
                }
            }
        }
    }
    
    if ((op_flags & OPT_D)) {
        if (daemon(1, 0)) {
            LOGFAT("Daemon failed: errno " << errno << endl);
            return 1;
        }
    }

    if (geteuid() == 0) {
        // Need to rewrite pid, it may have changed with the daemon call
        pidfile.write_pid();
        if (!logfilename.empty() && logfilename.compare("stderr")) {
            if (chown(logfilename.c_str(), runas, -1) < 0) {
                LOGERR("chown("<<logfilename<<") : errno : " << errno << endl);
            }
        }
        if (initgroups(upmpdcliuser.c_str(), runasg) < 0) {
            LOGERR("initgroup failed. Errno: " << errno << endl);
        }
        if (setuid(runas) < 0) {
            LOGFAT("Can't set my uid to " << runas << " current: " << geteuid()
                   << endl);
            return 1;
        }
#if 0        
        gid_t list[100];
        int ng = getgroups(100, list);
        cerr << "GROUPS: ";
        for (int i = 0; i < ng; i++) {
            cerr << int(list[i]) << " ";
        }
        cerr << endl;
#endif
    }

//// Dropped root 

    if (sc2mpdpath.empty()) {
        // Do we have an sc2mpd command installed (for songcast)?
        if (!ExecCmd::which("sc2mpd", sc2mpdpath))
            sc2mpdpath.clear();
    }
    if (senderpath.empty()) {
        // Do we have an scmakempdsender command installed (for
        // starting the songcast sender and its auxiliary mpd)?
        if (!ExecCmd::which("scmakempdsender", senderpath))
            senderpath.clear();
    }
    
    if (!sc2mpdpath.empty()) {
        // Check if sc2mpd is actually there
        if (access(sc2mpdpath.c_str(), X_OK|R_OK) != 0) {
            LOGERR("Specified path for sc2mpd: " << sc2mpdpath << 
                   " is not executable" << endl);
            sc2mpdpath.clear();
        }
    }

    if (!senderpath.empty()) {
        // Check that both the starter script and the mpd2sc sender
        // command are executable. We'll assume that mpd is ok
        if (access(senderpath.c_str(), X_OK|R_OK) != 0) {
            LOGERR("The specified path for the sender starter script: ["
                   << senderpath <<
                   "] is not executable, disabling the sender mode.\n");
            senderpath.clear();
        } else {
            string path;
            if (!ExecCmd::which("mpd2sc", path)) {
                LOGERR("Sender starter was specified and found but the mpd2sc "
                       "command is not found (or executable). Disabling "
                       "the sender mode.\n");
                senderpath.clear();
            }
        }
    }


    // Initialize MPD client object. Retry until it works or power fail.
    MPDCli *mpdclip = 0;
    int mpdretrysecs = 2;
    for (;;) {
        mpdclip = new MPDCli(mpdhost, mpdport, mpdpassword, onstart, onstop,
                             onvolumechange);
        if (mpdclip == 0) {
            LOGFAT("Can't allocate MPD client object" << endl);
            return 1;
        }
        if (!mpdclip->ok()) {
            LOGERR("MPD connection failed" << endl);
            delete mpdclip;
            mpdclip = 0;
            sleep(mpdretrysecs);
            mpdretrysecs = MIN(2*mpdretrysecs, 120);
        } else {
            break;
        }
    }

    // Initialize libupnpp, and check health
    LibUPnP *mylib = 0;
    string hwaddr;
    int libretrysecs = 10;
    for (;;) {
        // Libupnp init fails if we're started at boot and the network
        // is not ready yet. So retry this forever
        mylib = LibUPnP::getLibUPnP(true, &hwaddr, iface, upnpip, upport);
        if (mylib) {
            break;
        }
        sleep(libretrysecs);
        libretrysecs = MIN(2*libretrysecs, 120);
    }

    if (!mylib->ok()) {
        LOGFAT("Lib init failed: " <<
               mylib->errAsString("main", mylib->getInitError()) << endl);
        return 1;
    }

    if ((cp = getenv("UPMPDCLI_UPNPLOGFILENAME"))) {
        char *cp1 = getenv("UPMPDCLI_UPNPLOGLEVEL");
        int loglevel = LibUPnP::LogLevelNone;
        if (cp1) {
            loglevel = atoi(cp1);
        }
        loglevel = loglevel < 0 ? 0: loglevel;
        loglevel = loglevel > int(LibUPnP::LogLevelDebug) ? 
            int(LibUPnP::LogLevelDebug) : loglevel;

        if (loglevel != LibUPnP::LogLevelNone) {
            mylib->setLogFileName(cp, LibUPnP::LogLevel(loglevel));
        }
    }

    // Create unique ID
    string UUID = LibUPnP::makeDevUUID(friendlyname, hwaddr);

    // Initialize the data we serve through HTTP (device and service
    // descriptions, icons, presentation page, etc.)
    unordered_map<string, VDirContent> files;
    if (!initHttpFs(files, g_datadir, UUID, friendlyname, enableAV, enableOH,
                    !senderpath.empty(),
                    iconpath, presentationhtml)) {
        exit(1);
    }

    if (ownqueue)
        opts.options |= UpMpd::upmpdOwnQueue;
    if (enableOH)
        opts.options |= UpMpd::upmpdDoOH;
    if (ohmetapersist)
        opts.options |= UpMpd::upmpdOhMetaPersist;
    if (!sc2mpdpath.empty()) {
        opts.sc2mpdpath = sc2mpdpath;
        opts.options |= UpMpd::upmpdOhReceiver;
    }
    if (!senderpath.empty()) {
        opts.options |= UpMpd::upmpdOhSenderReceiver;
        opts.senderpath = senderpath;
        opts.sendermpdport = sendermpdport;
    }

    if (!enableAV)
        opts.options |= UpMpd::upmpdNoAV;
    // Initialize the UPnP device object.
    UpMpd device(string("uuid:") + UUID, friendlyname, 
                 files, mpdclip, opts);
    dev = &device;

    // And forever generate state change events.
    LOGDEB("Entering event loop" << endl);
    setupsigs();
    device.eventloop();
    LOGDEB("Event loop returned" << endl);

    return 0;
}
