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

#include "ohinfo.hxx"

#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS

#include <functional>                   // for _Bind, bind, _1, _2
#include <iostream>                     // for endl
#include <string>                       // for string
#include <unordered_map>                // for unordered_map, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/log.hxx"             // for LOGDEB
#include "libupnpp/soaphelp.hxx"        // for SoapOutgoing, i2s, SoapIncoming

#include "mpdcli.hxx"                   // for MpdStatus, etc
#include "upmpd.hxx"                    // for UpMpd
#include "upmpdutils.hxx"               // for didlmake, diffmaps

using namespace std;
using namespace std::placeholders;

static const string sTpProduct("urn:av-openhome-org:service:Info:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Info");

OHInfo::OHInfo(UpMpd *dev)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev)
{
    dev->addActionMapping(this, "Counters", 
                          bind(&OHInfo::counters, this, _1, _2));
    dev->addActionMapping(this, "Track", 
                          bind(&OHInfo::track, this, _1, _2));
    dev->addActionMapping(this, "Details", 
                          bind(&OHInfo::details, this, _1, _2));
    dev->addActionMapping(this, "Metatext", 
                          bind(&OHInfo::metatext, this, _1, _2));
}

void OHInfo::urimetadata(string& uri, string& metadata)
{
    const MpdStatus &mpds =  m_dev->getMpdStatusNoUpdate();
    bool is_song = (mpds.state == MpdStatus::MPDS_PLAY) || 
        (mpds.state == MpdStatus::MPDS_PAUSE);

    if (is_song) {
        uri = mpds.currentsong.uri;
        metadata = didlmake(mpds.currentsong);
    } else {
        uri.clear();
        metadata.clear();
    }
}

void OHInfo::makedetails(string &duration, string& bitrate, 
                         string& bitdepth, string& samplerate)
{
    const MpdStatus &mpds =  m_dev->getMpdStatusNoUpdate();

    bool is_song = (mpds.state == MpdStatus::MPDS_PLAY) || 
        (mpds.state == MpdStatus::MPDS_PAUSE);

    if (is_song) {
        duration = SoapHelp::i2s(mpds.songlenms / 1000);
        bitrate = SoapHelp::i2s(mpds.kbrate * 1000);
        bitdepth = SoapHelp::i2s(mpds.bitdepth);
        samplerate = SoapHelp::i2s(mpds.sample_rate);
    } else {
        duration = bitrate = bitdepth = samplerate = "0";
    }
}

bool OHInfo::makestate(unordered_map<string, string> &st)
{
    st.clear();

    st["TrackCount"] = SoapHelp::i2s(m_dev->m_mpds ? 
                                     m_dev->m_mpds->trackcounter : 0);
    st["DetailsCount"] = SoapHelp::i2s(m_dev->m_mpds ? 
                                       m_dev->m_mpds->detailscounter : 0);
    st["MetatextCount"] = "0";
    string uri, metadata;
    urimetadata(uri, metadata);
    st["Uri"] = uri;
    st["Metadata"] = metadata;
    makedetails(st["Duration"], st["BitRate"], st["BitDepth"],st["SampleRate"]);
    st["Lossless"] = "0";
    st["CodecName"] = "";
    st["Metatext"] = m_metatext;
    return true;
}

bool OHInfo::getEventData(bool all, std::vector<std::string>& names, 
                             std::vector<std::string>& values)
{
    //LOGDEB("OHInfo::getEventData" << endl);

    unordered_map<string, string> state, changed;
    makestate(state);
    if (all) {
        changed = state;
    } else {
        changed = diffmaps(m_state, state);
    }
    m_state = state;

    for (auto it = changed.begin(); it != changed.end(); it++) {
        names.push_back(it->first);
        values.push_back(it->second);
    }

    return true;
}

int OHInfo::counters(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHInfo::counters" << endl);
    
    data.addarg("TrackCount", SoapHelp::i2s(m_dev->m_mpds ?
                                            m_dev->m_mpds->trackcounter : 0));
    data.addarg("DetailsCount", SoapHelp::i2s(m_dev->m_mpds ?
                                              m_dev->m_mpds->detailscounter:0));
    data.addarg("MetatextCount", "0");
    return UPNP_E_SUCCESS;
}

int OHInfo::track(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHInfo::track" << endl);

    string uri, metadata;
    urimetadata(uri, metadata);
    data.addarg("Uri", uri);
    data.addarg("Metadata", metadata);
    return UPNP_E_SUCCESS;
}

int OHInfo::details(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHInfo::details" << endl);

    string duration, bitrate, bitdepth, samplerate;
    makedetails(duration, bitrate, bitdepth, samplerate);
    data.addarg("Duration", duration);
    data.addarg("BitRate", bitrate);
    data.addarg("BitDepth", bitdepth);
    data.addarg("SampleRate", samplerate);
    data.addarg("Lossless", "0");
    data.addarg("CodecName", "");
    return UPNP_E_SUCCESS;
}

int OHInfo::metatext(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHInfo::metatext" << endl);
    data.addarg("Value", m_state["Metatext"]);
    return UPNP_E_SUCCESS;
}

void OHInfo::setMetatext(const string& metatext)
{
    LOGDEB("OHInfo::setMetatext: " << metatext << endl);
    m_metatext = metatext;
}
