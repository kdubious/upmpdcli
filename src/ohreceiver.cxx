/* Copyright (C) 2014 J.F.Dockes
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

#include "ohreceiver.hxx"

#include <stdlib.h>                     // for atoi

#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS, etc

#include <functional>                   // for _Bind, bind, _1, _2
#include <iostream>                     // for endl, etc
#include <string>                       // for string, allocator, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/log.hxx"             // for LOGDEB, LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapArgs, SoapData, i2s, etc

#include "mpdcli.hxx"                   // for MpdStatus, UpSong, MPDCli, etc
#include "upmpd.hxx"                    // for UpMpd, etc
#include "upmpdutils.hxx"               // for didlmake, diffmaps, etc
#include "ohplaylist.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpProduct("urn:av-openhome-org:service:Receiver:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Receiver");

OHReceiver::OHReceiver(UpMpd *dev, OHPlaylist *pl, int port)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev), m_pl(pl), 
      m_httpport(port)
{
    dev->addActionMapping(this, "Play", 
                          bind(&OHReceiver::play, this, _1, _2));
    dev->addActionMapping(this, "Stop", 
                          bind(&OHReceiver::stop, this, _1, _2));
    dev->addActionMapping(this, "SetSender",
                          bind(&OHReceiver::setSender, this, _1, _2));
    dev->addActionMapping(this, "Sender", 
                          bind(&OHReceiver::sender, this, _1, _2));
    dev->addActionMapping(this, "ProtocolInfo",
                          bind(&OHReceiver::protocolInfo, this, _1, _2));
    dev->addActionMapping(this, "TransportState",
                          bind(&OHReceiver::transportState, this, _1, _2));

    m_httpuri = "http://localhost:"+ SoapHelp::i2s(m_httpport) + 
        "/Songcast.wav";
}

//                                 
static const string o_protocolinfo("ohz:*:*:*,ohm:*:*:*,ohu:*.*.*");
//static const string o_protocolinfo("ohu:*:*:*");

bool OHReceiver::makestate(unordered_map<string, string> &st)
{
    st.clear();

    st["Uri"] = m_uri;
    st["Metadata"] = m_metadata;
    // Allowed states: Stopped, Playing,Waiting, Buffering
    // We won't receive a Stop action if we are not Playing. So we
    // are playing as long as we have a subprocess
    if (m_cmd)
        st["TransportState"] = "Playing";
    else 
        st["TransportState"] = "Stopped";
    st["ProtocolInfo"] = o_protocolinfo;
    return true;
}

bool OHReceiver::getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values)
{
    //LOGDEB("OHReceiver::getEventData" << endl);

    unordered_map<string, string> state;

    makestate(state);

    unordered_map<string, string> changed;
    if (all) {
        changed = state;
    } else {
        changed = diffmaps(m_state, state);
    }
    m_state = state;

    for (auto it = changed.begin(); it != changed.end(); it++) {
        //LOGDEB("OHReceiver::getEventData: changed: " << it->first <<
        // " = " << it->second << endl);
        names.push_back(it->first);
        values.push_back(it->second);
    }

    return true;
}

void OHReceiver::maybeWakeUp(bool ok)
{
    if (ok && m_dev)
        m_dev->loopWakeup();
}

int OHReceiver::play(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHReceiver::play" << endl);
    bool ok = false;

    if (!m_pl) {
        LOGERR("OHReceiver::play: no playlist service" << endl);
        return UPNP_E_INTERNAL_ERROR;
    }
    if (m_uri.empty()) {
        LOGERR("OHReceiver::play: no uri" << endl);
        return UPNP_E_INTERNAL_ERROR;
    }
    if (m_metadata.empty()) {
        LOGERR("OHReceiver::play: no metadata" << endl);
        return UPNP_E_INTERNAL_ERROR;
    }

    m_dev->m_mpdcli->stop();

    int id = -1;
    unordered_map<int, string> urlmap;
    string line;
        
    // We start the songcast command to receive the audio flux and
    // export it as HTTP, then insert http URI at the front of the
    // queue and execute next/play
    if (m_cmd)
        m_cmd->zapChild();
    m_cmd = shared_ptr<ExecCmd>(new ExecCmd());
    vector<string> args;
    args.push_back("-u");
    args.push_back(m_uri);
    if (!g_configfilename.empty()) {
        args.push_back("-c");
        args.push_back(g_configfilename);
    }
        
    LOGDEB("OHReceiver::play: executing " << g_sc2mpd_path << endl);
    ok = m_cmd->startExec(g_sc2mpd_path, args, false, true) >= 0;
    if (!ok) {
        LOGERR("OHReceiver::play: executing " << g_sc2mpd_path << " failed" 
               << endl);
        goto out;
    } else {
        LOGDEB("OHReceiver::play: sc2mpd pid "<< m_cmd->getChildPid()<< endl);
    }
    // Wait for sc2mpd to signal ready, then play
    m_cmd->getline(line);
    LOGDEB("OHReceiver: sc2mpd sent: " << line);

    // And insert the appropriate uri in the mpd playlist
    if (!m_pl->urlMap(urlmap)) {
        LOGERR("OHReceiver::play: urlMap() failed" <<endl);
        goto out;
    }
    for (auto it = urlmap.begin(); it != urlmap.end(); it++) {
        if (it->second == m_httpuri) {
            id = it->first;
        }
    }
    if (id == -1) {
        ok = m_pl->insertUri(0, m_httpuri, SoapHelp::xmlUnquote(m_metadata),
            &id);
        if (!ok) {
            LOGERR("OHReceiver::play: insertUri() failed\n");
            goto out;
        }
    }

    ok = m_dev->m_mpdcli->playId(id);
    if (!ok) {
        LOGERR("OHReceiver::play: play() failed\n");
        goto out;
    }

out:
    if (!ok) {
        iStop();
    }
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

bool OHReceiver::iStop()
{
    if (m_cmd) {
        m_cmd->zapChild();
        m_cmd = shared_ptr<ExecCmd>(0);
    }
    m_dev->m_mpdcli->stop();

    unordered_map<int, string> urlmap;
    // Remove our bogus URi from the playlist
    if (!m_pl->urlMap(urlmap)) {
        LOGERR("OHReceiver::stop: urlMap() failed" <<endl);
        return false;
    }
    for (auto it = urlmap.begin(); it != urlmap.end(); it++) {
        if (it->second == m_httpuri) {
            m_dev->m_mpdcli->deleteId(it->first);
        }
    }
    return true;
}

int OHReceiver::stop(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHReceiver::stop" << endl);
    bool ok = iStop();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHReceiver::setSender(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHReceiver::setSender" << endl);
    string uri, metadata;
    bool ok = sc.getString("Uri", &uri) &&
        sc.getString("Metadata", &metadata);

    if (ok) {
        m_uri = uri;
        m_metadata = metadata;
        LOGDEB("OHReceiver::setSender: uri [" << m_uri << "] meta [" << 
               m_metadata << "]" << endl);
    }

    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHReceiver::sender(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHReceiver::sender" << endl);
    data.addarg("Uri", m_uri);
    data.addarg("Metadata", m_metadata);
    return UPNP_E_SUCCESS;
}

int OHReceiver::transportState(const SoapArgs& sc, SoapData& data)
{
    //LOGDEB("OHReceiver::transportState" << endl);

    // Allowed states: Stopped, Playing,Waiting, Buffering
    // We won't receive a Stop action if we are not Playing. So we
    // are playing as long as we have a subprocess
    string tstate = m_cmd ? "Playing" : "Stopped";
    data.addarg("Value", tstate);
    LOGDEB("OHReceiver::transportState: " << tstate << endl);
    return UPNP_E_SUCCESS;
}

int OHReceiver::protocolInfo(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHReceiver::protocolInfo" << endl);
    data.addarg("Value", o_protocolinfo);
    return UPNP_E_SUCCESS;
}
