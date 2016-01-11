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

#include "ohradio.hxx"

#include <stdlib.h>

#include <upnp/upnp.h>

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/base64.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpavutils.hxx"

#include "ohmetacache.hxx"
#include "mpdcli.hxx"
#include "upmpd.hxx"
#include "upmpdutils.hxx"
#include "conftree.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpProduct("urn:av-openhome-org:service:Radio:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Radio");

struct RadioMeta {
    RadioMeta(const string& t, const string& u)
        : title(t), uri(u) {
    }
    string title;
    string uri;
};

static vector<RadioMeta> o_radios;

OHRadio::OHRadio(UpMpd *dev)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev), m_id(0)
{
    dev->addActionMapping(this, "Channel",
                          bind(&OHRadio::channel, this, _1, _2));
    dev->addActionMapping(this, "ChannelsMax",
                          bind(&OHRadio::channelsMax, this, _1, _2));
    dev->addActionMapping(this, "Id",
                          bind(&OHRadio::id, this, _1, _2));
    dev->addActionMapping(this, "IdArray",
                          bind(&OHRadio::idArray, this, _1, _2));
    dev->addActionMapping(this, "IdArrayChanged",
                          bind(&OHRadio::idArrayChanged, this, _1, _2));
    dev->addActionMapping(this, "Pause",
                          bind(&OHRadio::pause, this, _1, _2));
    dev->addActionMapping(this, "Play",
                          bind(&OHRadio::play, this, _1, _2));
    dev->addActionMapping(this, "ProtocolInfo",
                          bind(&OHRadio::protocolInfo, this, _1, _2));
    dev->addActionMapping(this, "Read",
                          bind(&OHRadio::ohread, this, _1, _2));
    dev->addActionMapping(this, "ReadList",
                          bind(&OHRadio::readList, this, _1, _2));
    dev->addActionMapping(this, "SeekSecondAbsolute",
                          bind(&OHRadio::seekSecondAbsolute, this, _1, _2));
    dev->addActionMapping(this, "SeekSecondRelative",
                          bind(&OHRadio::seekSecondRelative, this, _1, _2));
    dev->addActionMapping(this, "SetChannel",
                          bind(&OHRadio::setChannel, this, _1, _2));
    dev->addActionMapping(this, "SetId",
                          bind(&OHRadio::setId, this, _1, _2));
    dev->addActionMapping(this, "Stop",
                          bind(&OHRadio::stop, this, _1, _2));
    dev->addActionMapping(this, "TransportState",
                          bind(&OHRadio::transportState, this, _1, _2));
    readRadios();
}

void OHRadio::readRadios()
{
    // Id 0 means no selection
    o_radios.push_back(RadioMeta("Unknown radio", ""));
    
    vector<string> allsubk = g_config->getSubKeys();
    for (auto it = allsubk.begin(); it != allsubk.end(); it++) {
        LOGDEB("OHRadio::readRadios: subk " << *it << endl);
        if (it->find("radio ") == 0) {
            string uri;
            string title = it->substr(6);
            bool ok = g_config->get("url", uri, *it);
            if (ok && !uri.empty()) {
                o_radios.push_back(RadioMeta(title, uri));
                LOGDEB("OHRadio::readRadios:RADIO: [" <<
                       title << "] uri [" << uri << "]\n");
            }
        }
    }
    LOGDEB("OHRadio::readRadios: " << o_radios.size() << " radios found\n")
}

static const int channelsmax = 200;

static string mpdstatusToTransportState(MpdStatus::State st)
{
    string tstate;
    switch (st) {
    case MpdStatus::MPDS_PLAY:
        tstate = "Playing";
        break;
    case MpdStatus::MPDS_PAUSE:
        tstate = "Paused";
        break;
    default:
        tstate = "Stopped";
    }
    return tstate;
}

// The data format for id lists is an array of msb 32 bits ints
// encoded in base64...
bool OHRadio::makeIdArray(string& out)
{
    LOGDEB1("OHRadio::makeIdArray\n");
    string out1;
    string sdeb;
    for (unsigned int val = 1; val < o_radios.size(); val++) {
        out1 += (unsigned char) ((val & 0xff000000) >> 24);
        out1 += (unsigned char) ((val & 0x00ff0000) >> 16);
        out1 += (unsigned char) ((val & 0x0000ff00) >> 8);
        out1 += (unsigned char) ((val & 0x000000ff));
        sdeb += SoapHelp::i2s(val) + " ";
    }
    LOGDEB("OHRadio::translateIdArray: current ids: " << sdeb << endl);
    out = base64_encode(out1);
    return true;
}

bool OHRadio::makestate(unordered_map<string, string>& st)
{
    st.clear();

    const MpdStatus& mpds = m_dev->getMpdStatusNoUpdate();

    st["ChannelsMax"] = SoapHelp::i2s(channelsmax);
    st["Id"] = SoapHelp::i2s(m_id);
    makeIdArray(st["IdArray"]);
    if (m_id && m_id < o_radios.size()) {
        UpSong song;
        song.album = o_radios[m_id].title;
        song.uri = o_radios[m_id].uri;
        st["Metadata"] =  didlmake(song);
    } else {
        st["Metadata"] =  "";
    }
    st["ProtocolInfo"] = g_protocolInfo;
    st["TransportState"] =  mpdstatusToTransportState(mpds.state);
    st["Uri"] = mpds.currentsong.uri;
    return true;
}

bool OHRadio::getEventData(bool all, std::vector<std::string>& names,
                           std::vector<std::string>& values)
{
    //LOGDEB("OHRadio::getEventData" << endl);

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
        names.push_back(it->first);
        values.push_back(it->second);
    }

    return true;
}

void OHRadio::maybeWakeUp(bool ok)
{
    if (ok && m_dev) {
        m_dev->loopWakeup();
    }
}

int OHRadio::channel(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::channel" << endl);
    const MpdStatus& mpds = m_dev->getMpdStatusNoUpdate();
    string metadata = didlmake(mpds.currentsong);
    data.addarg("Uri", mpds.currentsong.uri);
    data.addarg("Metadata", metadata);

    return UPNP_E_SUCCESS;
}

int OHRadio::channelsMax(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::channelsMax" << endl);
    data.addarg("Value", SoapHelp::i2s(channelsmax));
    return UPNP_E_SUCCESS;
}

int OHRadio::play(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::play" << endl);
    m_dev->m_mpdcli->consume(false);
    m_dev->m_mpdcli->single(false);
    bool ok = m_dev->m_mpdcli->play();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::pause(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::pause" << endl);
    bool ok = m_dev->m_mpdcli->pause(true);
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::iStop()
{
    bool ok = m_dev->m_mpdcli->stop();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}
int OHRadio::stop(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::stop" << endl);
    return iStop();
}

int OHRadio::seekSecondAbsolute(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::seekSecondAbsolute" << endl);
    int seconds;
    bool ok = sc.get("Value", &seconds);
    if (ok) {
        ok = m_dev->m_mpdcli->seek(seconds);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::seekSecondRelative(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::seekSecondRelative" << endl);
    int seconds;
    bool ok = sc.get("Value", &seconds);
    if (ok) {
        const MpdStatus& mpds =  m_dev->getMpdStatusNoUpdate();
        bool is_song = (mpds.state == MpdStatus::MPDS_PLAY) ||
                       (mpds.state == MpdStatus::MPDS_PAUSE);
        if (is_song) {
            seconds += mpds.songelapsedms / 1000;
            ok = m_dev->m_mpdcli->seek(seconds);
        } else {
            ok = false;
        }
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::transportState(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::transportState" << endl);
    const MpdStatus& mpds = m_dev->getMpdStatusNoUpdate();
    string tstate;
    switch (mpds.state) {
    case MpdStatus::MPDS_PLAY:
        tstate = "Playing";
        break;
    case MpdStatus::MPDS_PAUSE:
        tstate = "Paused";
        break;
    default:
        tstate = "Stopped";
    }
    data.addarg("Value", tstate);
    return UPNP_E_SUCCESS;
}

int OHRadio::setChannel(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::setId" << endl);
    string uri, metadata;
    bool ok = sc.get("Uri", &uri);
    if (ok) {
        ok = sc.get("Metadata", &metadata);
        m_id = 0;
        // Do something
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::setId(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::setId" << endl);
    int id;
    bool ok = sc.get("Value", &id);
    if (ok) {
        m_id = id;
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Return current Id. Not the same as for playlist, this is our internal channel
// id, nothing to do with mpd's
int OHRadio::id(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::id" << endl);
    const MpdStatus& mpds = m_dev->getMpdStatusNoUpdate();
    data.addarg("Value", mpds.songid == -1 ? "0" : SoapHelp::i2s(mpds.songid));
    return UPNP_E_SUCCESS;
}

// Report the uri and metadata for a given channel id.
int OHRadio::ohread(const SoapIncoming& sc, SoapOutgoing& data)
{
    int id;
    bool ok = sc.get("Id", &id);
    LOGDEB("OHRadio::ohread id " << id << endl);
    if (ok) {
        if (id > 0 && id  < int(o_radios.size())) {
            data.addarg("Uri", o_radios[id].uri);
            UpSong song;
            song.album = o_radios[id].title;
            song.uri = o_radios[id].uri;
            data.addarg("Metadata", didlmake(song));
        } else {
            ok = false;
        }
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Given a space separated list of track Id's, report their associated
// uri and metadata in the following xml form:
//
//  <TrackList>
//    <Entry>
//      <Id></Id>
//      <Uri></Uri>
//      <Metadata></Metadata>
//    </Entry>
//  </TrackList>
//
// Any ids not in the radio are ignored.
int OHRadio::readList(const SoapIncoming& sc, SoapOutgoing& data)
{
    string sids;
    UpSong song;

    bool ok = sc.get("IdList", &sids);
    LOGDEB("OHRadio::readList: [" << sids << "]" << endl);

    vector<string> ids;
    string out("<ChannelList>");
    if (ok) {
        stringToTokens(sids, ids);
        for (auto it = ids.begin(); it != ids.end(); it++) {
            int id = atoi(it->c_str());
            if (id <= 0 || id >= int(o_radios.size())) {
                LOGDEB("OHRadio::readlist: bad id " << id << endl);
                continue;
            }
            song.title = o_radios[id].title;
            song.uri = o_radios[id].uri;
            LOGDEB("OHRadio::readlist: get data for  " << id << " alb " <<
                   song.album << " uri " << song.uri << endl);
            out += "<Entry><Id>";
            out += *it;
            out += "</Id><Metadata>";
            out += SoapHelp::xmlQuote(didlmake(song));
            out += "</Metadata></Entry>";
        }
        out += "</ChannelList>";
        LOGDEB("OHRadio::readList: out: [" << out << "]" << endl);
        data.addarg("ChannelList", out);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Returns current list of id as array of big endian 32bits integers,
// base-64-encoded.
int OHRadio::idArray(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::idArray" << endl);
    string idarray;
    if (makeIdArray(idarray)) {
        data.addarg("Token", SoapHelp::i2s(1));
        data.addarg("Array", idarray);
        return UPNP_E_SUCCESS;
    }
    return UPNP_E_INTERNAL_ERROR;
}

// Check if id array changed since last call (which returned a gen token)
int OHRadio::idArrayChanged(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::idArrayChanged" << endl);
    data.addarg("Value", SoapHelp::i2s(0));

    return UPNP_E_SUCCESS;
}

int OHRadio::protocolInfo(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::protocolInfo" << endl);
    data.addarg("Value", g_protocolInfo);
    return UPNP_E_SUCCESS;
}
