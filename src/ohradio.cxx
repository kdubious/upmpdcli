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

#include "mpdcli.hxx"
#include "upmpd.hxx"
#include "upmpdutils.hxx"
#include "conftree.hxx"
#include "execmd.h"
#include "ohproduct.hxx"
#include "ohinfo.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpProduct("urn:av-openhome-org:service:Radio:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Radio");

struct RadioMeta {
    RadioMeta(const string& t, const string& u, const string& au)
        : title(t), uri(u), artUri(au) {
    }
    string title;
    string uri;
    string artUri;
};

static vector<RadioMeta> o_radios;

OHRadio::OHRadio(UpMpd *dev)
    : OHService(sTpProduct, sIdProduct, dev), m_active(false),
      m_id(0), m_ok(false)
{
    // Need Python
    string pypath;
    if (!ExecCmd::which("python2", pypath)) {
        LOGINF("OHRadio: python2 not found, no radio service will be created\n");
        return;
    }
    if (!readRadios()) {
        LOGINF("OHRadio: readRadios() failed, no radio service will be created\n");
        return;
    }
    m_ok = true;
    
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
}

bool OHRadio::readRadios()
{
    // Id 0 means no selection
    o_radios.push_back(RadioMeta("Unknown radio", "", ""));
    
    UPnPP::PTMutexLocker conflock(g_configlock);
    vector<string> allsubk = g_config->getSubKeys_unsorted();
    for (auto it = allsubk.begin(); it != allsubk.end(); it++) {
        LOGDEB("OHRadio::readRadios: subk " << *it << endl);
        if (it->find("radio ") == 0) {
            string uri, artUri;
            string title = it->substr(6);
            bool ok = g_config->get("url", uri, *it);
            g_config->get("artUrl", artUri, *it);
            if (ok && !uri.empty()) {
                o_radios.push_back(RadioMeta(title, uri, artUri));
                LOGDEB("OHRadio::readRadios:RADIO: [" << title << "] uri [" <<
                       uri << "] artUri [" << artUri << "]\n");
            }
        }
    }
    LOGDEB("OHRadio::readRadios: " << o_radios.size() << " radios found\n");
    return true;
}

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
// encoded in base64. The values could be anything, but, for us, they
// are just the indices into o_radios(), beginning at 1 because 0 is
// special (it's reserved in o_radios too).
bool OHRadio::makeIdArray(string& out)
{
    //LOGDEB1("OHRadio::makeIdArray\n");
    string out1;
    for (unsigned int val = 1; val < o_radios.size(); val++) {
        out1 += (unsigned char) ((val & 0xff000000) >> 24);
        out1 += (unsigned char) ((val & 0x00ff0000) >> 16);
        out1 += (unsigned char) ((val & 0x0000ff00) >> 8);
        out1 += (unsigned char) ((val & 0x000000ff));
    }
    out = base64_encode(out1);
    return true;
}

bool OHRadio::makestate(unordered_map<string, string>& st)
{
    st.clear();

    MpdStatus mpds = m_dev->getMpdStatusNoUpdate();

    st["ChannelsMax"] = SoapHelp::i2s(o_radios.size());
    st["Id"] = SoapHelp::i2s(m_id);
    makeIdArray(st["IdArray"]);
    if (m_active && m_id >= 0 && m_id < o_radios.size()) {
        if (mpds.currentsong.album.empty()) {
            mpds.currentsong.album = o_radios[m_id].title;
        }
        mpds.currentsong.artUri = o_radios[m_id].artUri;
        string meta = didlmake(mpds.currentsong);
        st["Metadata"] =  meta;
        m_dev->m_ohif->setMetatext(meta);
    } else {
        if (m_active) 
            LOGDEB("OHRadio::makestate: bad m_id " << m_id << endl);
        st["Metadata"] =  "";
        m_dev->m_ohif->setMetatext("");
    }
    st["ProtocolInfo"] = g_protocolInfo;
    st["TransportState"] =  mpdstatusToTransportState(mpds.state);
    st["Uri"] = mpds.currentsong.uri;
    return true;
}

void OHRadio::maybeWakeUp(bool ok)
{
    if (ok && m_dev) {
        m_dev->loopWakeup();
    }
}

int OHRadio::setPlaying()
{
    if (m_id > o_radios.size() || o_radios[m_id].uri.empty()) {
        LOGERR("OHRadio::setPlaying: called with bad id (" << m_id <<
               ") or empty preset uri [" << o_radios[m_id].uri << "]\n");
        return UPNP_E_INTERNAL_ERROR;
    }
    
    string cmdpath = path_cat(g_datadir, "rdpl2stream");
    cmdpath = path_cat(cmdpath, "fetchStream.py");

    // Execute the playlist parser
    ExecCmd cmd;
    vector<string> args;
    args.push_back(o_radios[m_id].uri);
    LOGDEB("OHRadio::setPlaying: exec: " << cmdpath << " " << args[0] << endl);
    if (cmd.startExec(cmdpath, args, false, true) < 0) {
        LOGDEB("OHRadio::setPlaying: startExec failed for " <<
               cmdpath << " " << args[0] << endl);
        return UPNP_E_INTERNAL_ERROR;
    }

    // Read actual audio stream url
    string audiourl;
    if (cmd.getline(audiourl, 10) < 0) {
        LOGDEB("OHRadio::setPlaying: could not get audio url\n");
        return UPNP_E_INTERNAL_ERROR;
    }
    trimstring(audiourl, "\r\n");
    if (audiourl.empty()) {
        LOGDEB("OHRadio::setPlaying: audio url empty\n");
        return UPNP_E_INTERNAL_ERROR;
    }

    // Send url to mpd
    m_dev->m_mpdcli->clearQueue();
    UpSong song;
    song.album = o_radios[m_id].title;
    song.uri = o_radios[m_id].uri;
    if (m_dev->m_mpdcli->insert(audiourl, 0, song) < 0) {
        LOGDEB("OHRadio::setPlaying: mpd insert failed\n");
        return UPNP_E_INTERNAL_ERROR;
    }
    m_dev->m_mpdcli->single(true);
    if (!m_dev->m_mpdcli->play(0)) {
        LOGDEB("OHRadio::setPlaying: mpd play failed\n");
        return UPNP_E_INTERNAL_ERROR;
    }
    return UPNP_E_SUCCESS;
}

void OHRadio::setActive(bool onoff) {
    m_active = onoff;
    if (m_active) {
        if (m_id)
            m_dev->m_mpdcli->restoreState(m_mpdsavedstate);
        maybeWakeUp(true);
    } else {
        m_dev->m_mpdcli->saveState(m_mpdsavedstate);
        m_dev->m_mpdcli->clearQueue();
        iStop();
    }
}

int OHRadio::iPlay()
{
    int ret = setPlaying();
    maybeWakeUp(ret == UPNP_E_SUCCESS);
    return ret;
}

int OHRadio::play(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::play" << endl);
    if (!m_active && m_dev->m_ohpr) {
        m_dev->m_ohpr->iSetSourceIndexByName("Radio");
    }
    return iPlay();
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

int OHRadio::channel(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::channel" << endl);
    data.addarg("Uri", m_state["Uri"]);
    data.addarg("Metadata", m_state["Metadata"]);
    return UPNP_E_SUCCESS;
}

int OHRadio::setChannel(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::setChannel" << endl);
    string uri, metadata;
    bool ok = sc.get("Uri", &uri) && sc.get("Metadata", &metadata);
    if (ok) {
        iStop();
        m_id = 0;
        o_radios[0].uri = uri;
        UpSong ups;
        uMetaToUpSong(metadata, &ups);
        o_radios[0].title = ups.album + " " + ups.title;
    }
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHRadio::setId(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::setId" << endl);
    int id;
    if (!sc.get("Value", &id)) {
        LOGDEB("OHRadio::setId: no value ??\n");
        return UPNP_E_INTERNAL_ERROR;
    }
    if (id <= 0 || id > int(o_radios.size())) {
        LOGDEB("OHRadio::setId: bad value " << id << endl);
        return UPNP_E_INTERNAL_ERROR;
    }
    iStop();
    m_id = id;
    maybeWakeUp(true);
    return UPNP_E_SUCCESS;
}

// Return current channel Id. 
int OHRadio::id(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::id" << endl);
    data.addarg("Value", SoapHelp::i2s(m_id));
    return UPNP_E_SUCCESS;
}

static string radioDidlMake(const string& title, const string& uri,
                            const string& artUri)
{
    string out("<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
               "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\"\n"
               "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">\n"
               "<item id=\"\" parentID=\"\" restricted=\"True\">\n"
               "<dc:title>");
    out += SoapHelp::xmlQuote(title);
    out += "</dc:title>\n"
        "<res protocolInfo=\"*:*:*:*\" bitrate=\"6000\">";
    out += SoapHelp::xmlQuote(uri);
    out += "</res>\n"
        "<upnp:albumArtURI>";
    out += SoapHelp::xmlQuote(artUri);
    out += "</upnp:albumArtURI>\n"
        "<upnp:class>object.item.audioItem</upnp:class>\n"
        "</item>\n"
        "</DIDL-Lite>\n";
    return out;
}

string OHRadio::metaForId(unsigned int id)
{
    string meta;
    if (id >= 0 && id  < o_radios.size()) {
        if (0 && id == m_id) {
            meta = m_state["Metadata"];
        } else {
            meta = radioDidlMake(o_radios[id].title, o_radios[id].uri, 
                                 o_radios[id].artUri);
        }
    }
    return meta;
}

// Report the uri and metadata for a given channel id.
int OHRadio::ohread(const SoapIncoming& sc, SoapOutgoing& data)
{
    int id;
    bool ok = sc.get("Id", &id);
    if (ok) {
        LOGDEB("OHRadio::read id " << id << endl);
        if (id >= 0 && id  < int(o_radios.size())) {
            string meta = metaForId(id);
            data.addarg("Metadata", meta);
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
            string meta = metaForId(id);
            out += "<Entry><Id>";
            out += *it;
            out += "</Id><Uri>";
            out += SoapHelp::xmlQuote(o_radios[id].uri);
            out += "</Uri><Metadata>";
            out += SoapHelp::xmlQuote(meta);
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

// Check if id array changed since last call (which returned a gen
// token). Our array never changes
int OHRadio::idArrayChanged(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::idArrayChanged" << endl);
    data.addarg("Value", SoapHelp::i2s(0));
    return UPNP_E_SUCCESS;
}

int OHRadio::channelsMax(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::channelsMax" << endl);
    data.addarg("Value", SoapHelp::i2s(o_radios.size()));
    return UPNP_E_SUCCESS;
}

int OHRadio::protocolInfo(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHRadio::protocolInfo" << endl);
    data.addarg("Value", g_protocolInfo);
    return UPNP_E_SUCCESS;
}
