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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <set>
using namespace std;
using namespace std::placeholders;

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device.hxx"
#include "libupnpp/log.hxx"

#include "upmpd.hxx"
#include "ohinfo.hxx"
#include "mpdcli.hxx"
#include "upmpdutils.hxx"

static const string sTpProduct("urn:av-openhome-org:service:Info:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Info");

OHInfo::OHInfo(UpMpd *dev)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev)
{
    dev->addActionMapping("Counters", bind(&OHInfo::counters,
                                           this, _1, _2));
    dev->addActionMapping("Track", bind(&OHInfo::track,
                                           this, _1, _2));
    dev->addActionMapping("Details", bind(&OHInfo::details,
                                          this, _1, _2));
    dev->addActionMapping("Metatext", bind(&OHInfo::metatext,
                                          this, _1, _2));
}

bool OHInfo::makestate(unordered_map<string, string> &st)
{
    st.clear();

    char cbuf[30];
    sprintf(cbuf, "%d", m_dev->m_mpds ? m_dev->m_mpds->trackcounter : 0);
    st["TrackCount"] = cbuf;
    sprintf(cbuf, "%d", m_dev->m_mpds ? m_dev->m_mpds->detailscounter : 0);
    st["DetailsCount"] = cbuf;
    st["MetatextCount"] = "0";
    string uri, metadata;
    urimetadata(uri, metadata);
    st["Uri"] = uri;
    st["Metadata"] = metadata;
    string duration("0"), bitrate("0"), bitdepth("0"), samplerate("0");
    makedetails(duration, bitrate, bitdepth, samplerate);
    st["Duration"] = duration;
    st["BitRate"] = bitrate;
    st["BitDepth"] = bitdepth;
    st["SampleRate"] = samplerate;
    st["Lossless"] = "0";
    st["CodecName"] = "";

    st["Metatext"] = "";
    return true;
}

bool OHInfo::getEventData(bool all, std::vector<std::string>& names, 
                             std::vector<std::string>& values)
{
    //LOGDEB("OHInfo::getEventData" << endl);

    unordered_map<string, string> state;
    makestate(state);

    unordered_map<string, string> changed;
    if (all) {
        changed = state;
    } else {
        changed = diffmaps(m_state, state);
    }
    m_state = state;

    for (unordered_map<string, string>::iterator it = changed.begin();
         it != changed.end(); it++) {
        names.push_back(it->first);
        values.push_back(it->second);
    }

    return true;
}

int OHInfo::counters(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHInfo::counters" << endl);
    char cbuf[30];
    sprintf(cbuf, "%d", m_dev->m_mpds ? m_dev->m_mpds->trackcounter : 0);
    data.addarg("TrackCount", cbuf);
    sprintf(cbuf, "%d", m_dev->m_mpds ? m_dev->m_mpds->detailscounter : 0);
    data.addarg("DetailsCount", cbuf);
    data.addarg("MetatextCount", "0");
    return UPNP_E_SUCCESS;
}

void OHInfo::urimetadata(string& uri, string& metadata)
{
    const MpdStatus &mpds =  m_dev->getMpdStatus();
    bool is_song = (mpds.state == MpdStatus::MPDS_PLAY) || 
        (mpds.state == MpdStatus::MPDS_PAUSE);

    if (is_song) {
        uri = mpds.currentsong.uri;
        metadata = didlmake(mpds.currentsong);
    }
}

int OHInfo::track(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHInfo::track" << endl);

    string uri, metadata;
    urimetadata(uri, metadata);
    data.addarg("Uri", uri);
    data.addarg("Metadata", metadata);
    return UPNP_E_SUCCESS;
}

void OHInfo::makedetails(string &duration, string& bitrate, 
                         string& bitdepth, string& samplerate)
{
    const MpdStatus &mpds =  m_dev->getMpdStatus();

    bool is_song = (mpds.state == MpdStatus::MPDS_PLAY) || 
        (mpds.state == MpdStatus::MPDS_PAUSE);

    char cbuf[30];
    if (is_song) {
        sprintf(cbuf, "%u", mpds.songlenms / 1000);
        duration = cbuf;
        sprintf(cbuf, "%u", mpds.kbrate * 1000);
        bitrate = cbuf;
        sprintf(cbuf, "%u", mpds.bitdepth);
        bitdepth = cbuf;
        sprintf(cbuf, "%u", mpds.sample_rate);
        samplerate = cbuf;
    }
}

int OHInfo::details(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHInfo::details" << endl);

    string duration("0"), bitrate("0"), bitdepth("0"), samplerate("0");
    makedetails(duration, bitrate, bitdepth, samplerate);
    data.addarg("Duration", duration);
    data.addarg("BitRate", bitrate);
    data.addarg("BitDepth", bitdepth);
    data.addarg("SampleRate", samplerate);
    data.addarg("Lossless", "0");
    data.addarg("CodecName", "");
    return UPNP_E_SUCCESS;
}

int OHInfo::metatext(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHInfo::metatext" << endl);
    data.addarg("Value", "");
    return UPNP_E_SUCCESS;
}
