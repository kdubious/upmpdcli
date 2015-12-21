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

static const string makesendercmd("scmakempdsender.py");
static int mpdport = 6700;

class SenderReceiver::Internal {
public:
    Internal(UpMpd *dv)
        : dev(dv), mpd(0), origmpd(0), cmd(0) {
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
        delete cmd;
    }
    UpMpd *dev;
    MPDCli *mpd;
    MPDCli *origmpd;
    ExecCmd *cmd;
    string uri;
    string meta;
};


SenderReceiver::SenderReceiver(UpMpd *dev)
{
    m = new Internal(dev);
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

    // Playing state. If playing is stopped at this point elapsedms is
    // lost which is why we get it as a parameter
    MpdStatus mpds = src->getStatus();
    if (seekms < 0) {
        seekms = mpds.songelapsedms;
    }
    // Playlist from source mpd
    vector<UpSong> playlist;
    if (!src->getQueueData(playlist)) {
        LOGERR("copyMpd: can't retrieve current playlist\n");
        return false;
    }
    // Copy the playlist to dest
    dest->clearQueue();
    for (unsigned int i = 0; i < playlist.size(); i++) {
        if (dest->insert(playlist[i].uri, i, playlist[i]) < 0) {
            LOGERR("copyMpdt: mpd->insert failed\n");
            return false;
        }
    }
    dest->play(mpds.songpos);
    dest->setVolume(mpds.volume);
    dest->seek(seekms/1000);
    return true;
}

bool SenderReceiver::start(int seekms)
{
    LOGDEB("SenderReceiver::start. seekms " << seekms << endl);
    
    if (!m->dev || !m->dev->m_ohpl) {
        LOGERR("SenderReceiver::start: no ohpl\n");
        return false;
    }
    
    // Stop MPD Play (normally already done)
    m->dev->m_mpdcli->stop();

    if (!m->cmd) {
        // First time: Start fifo MPD and Sender
        m->cmd = new ExecCmd();
        vector<string> args;
        args.push_back("-p");
        args.push_back(SoapHelp::i2s(mpdport));
        args.push_back("-f");
        args.push_back(m->dev->m_friendlyname);
        m->cmd->startExec(makesendercmd, args, false, true);

        string output;
        if (!m->cmd->getline(output)) {
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
        m->mpd = new MPDCli("localhost", mpdport);
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
