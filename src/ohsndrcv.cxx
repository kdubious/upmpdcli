/* Copyright (C) 2015 J.F.Dockes
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

#include "ohsndrcv.hxx"

#include "libupnpp/log.hxx"
#include "libupnpp/base64.hxx"

#include "execmd.h"
#include "upmpd.hxx"
#include "mpdcli.hxx"
#include "upmpdutils.hxx"
#include "ohreceiver.hxx"
#include "ohplaylist.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

class SenderReceiver::Internal {
public:
    Internal(UpMpd *dv, const string& starterpath, int port)
        : dev(dv), mpd(0), origmpd(0), sender(0), makesendercmd(starterpath),
          mpdport(port) {
    }
    ~Internal() {
        clear();
    }
    void clear() {
        if (dev && origmpd) {
            dev->m_mpdcli = origmpd;
            origmpd = 0;
            if (dev->m_ohrcv) {
                dev->m_ohrcv->iStop();
            }
            if (dev->m_ohpl) {
                dev->m_ohpl->refreshState();
            }
        }
        delete mpd;
        mpd = 0;
        delete sender;
        sender = 0;
    }
    UpMpd *dev;
    MPDCli *mpd;
    MPDCli *origmpd;
    ExecCmd *sender;
    string uri;
    string meta;
    string makesendercmd;
    int mpdport;
};


SenderReceiver::SenderReceiver(UpMpd *dev, const string& starterpath, int port)
{
    m = new Internal(dev, starterpath, port);
}

SenderReceiver::~SenderReceiver()
{
    if (m)
        delete m;
}

static bool copyMpd(MPDCli *src, MPDCli *dest, int seekms)
{
    if (!src || !dest) {
        LOGERR("copyMpd: src or dest is null\n");
        return false;
    }
    MpdState st;
    return src->saveState(st, seekms) && dest->restoreState(st);
}

bool SenderReceiver::start(bool useradio, int seekms)
{
    LOGDEB("SenderReceiver::start. seekms " << seekms << endl);
    
    if (!m->dev || !m->dev->m_ohpl) {
        LOGERR("SenderReceiver::start: no dev or ohpl??\n");
        return false;
    }
    
    // Stop MPD Play (normally already done)
    m->dev->m_mpdcli->stop();

    if (!m->sender) {
        // First time: Start fifo MPD and Sender
        m->sender = new ExecCmd();
        vector<string> args;
        args.push_back("-p");
        args.push_back(SoapHelp::i2s(m->mpdport));
        args.push_back("-f");
        args.push_back(m->dev->m_friendlyname);
        m->sender->startExec(m->makesendercmd, args, false, true);

        string output;
        if (m->sender->getline(output) <= 0) {
            LOGERR("SenderReceiver::start: makesender command failed\n");
            m->clear();
            return false;
        }
        LOGDEB("SenderReceiver::start got [" << output << "] from script\n");

        // Output is like [Ok mpdport URI base64-encoded-uri METADATA b64-meta]
        vector<string> toks;
        stringToTokens(output, toks);
        if (toks.size() != 6 || toks[0].compare("Ok")) {
            LOGERR("SenderReceiver::start: bad output from script: " << output
                   << endl);
            m->clear();
            return false;
        }
        m->uri = base64_decode(toks[3]);
        m->meta = base64_decode(toks[5]);

        // Connect to the new MPD
        m->mpd = new MPDCli("localhost", m->mpdport);
        if (!m->mpd || !m->mpd->ok()) {
            LOGERR("SenderReceiver::start: can't connect to new MPD\n");
            m->clear();
            return false;
        }
    }
    
    // Start our receiver
    if (!m->dev->m_ohrcv->iSetSender(m->uri, m->meta) ||
        !m->dev->m_ohrcv->iPlay()) {
        m->clear();
        return false;
    }

    // Copy mpd state 
    copyMpd(m->dev->m_mpdcli, m->mpd, seekms);
    m->origmpd = m->dev->m_mpdcli;
    m->dev->m_mpdcli = m->mpd;

    return true;
}

bool SenderReceiver::stop()
{
    LOGDEB("SenderReceiver::stop()\n");
    // Do we want to transfer the playlist back ? Probably we do.
    if (!m->dev || !m->origmpd || !m->mpd || !m->dev->m_ohpl ||
        !m->dev->m_ohrcv) {
        LOGERR("SenderReceiver::stop: bad state: dev/origmpd/mpd null\n");
        return false;
    }
    copyMpd(m->mpd, m->origmpd, -1);
    m->mpd->stop();
    m->dev->m_mpdcli = m->origmpd;
    m->origmpd = 0;
    m->dev->m_ohrcv->iStop();
    m->dev->m_ohpl->refreshState();

    return true;
}
