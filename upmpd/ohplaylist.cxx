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
#include "ohplaylist.hxx"
#include "mpdcli.hxx"
#include "upmpdutils.hxx"
//#include "renderctl.hxx"
#include "base64.hxx"

static const string sTpProduct("urn:av-openhome-org:service:Playlist:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Playlist");

OHPlaylist::OHPlaylist(UpMpd *dev, UpMpdRenderCtl *ctl)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev), m_ctl(ctl)
{
    dev->addActionMapping(this, "Play", 
                          bind(&OHPlaylist::play, this, _1, _2));
    dev->addActionMapping(this, "Pause", 
                          bind(&OHPlaylist::pause, this, _1, _2));
    dev->addActionMapping(this, "Stop", 
                          bind(&OHPlaylist::stop, this, _1, _2));
    dev->addActionMapping(this, "Next", 
                          bind(&OHPlaylist::next, this, _1, _2));
    dev->addActionMapping(this, "Previous", 
                          bind(&OHPlaylist::previous, this, _1, _2));
    dev->addActionMapping(this, "SetRepeat",
                          bind(&OHPlaylist::setRepeat, this, _1, _2));
    dev->addActionMapping(this, "Repeat",
                          bind(&OHPlaylist::repeat, this, _1, _2));
    dev->addActionMapping(this, "SetShuffle",
                          bind(&OHPlaylist::setShuffle, this, _1, _2));
    dev->addActionMapping(this, "Shuffle",
                          bind(&OHPlaylist::shuffle, this, _1, _2));
    dev->addActionMapping(this, "SeekSecondAbsolute",
                          bind(&OHPlaylist::seekSecondAbsolute, this, _1, _2));
    dev->addActionMapping(this, "SeekSecondRelative",
                          bind(&OHPlaylist::seekSecondRelative, this, _1, _2));
    dev->addActionMapping(this, "SeekId",
                          bind(&OHPlaylist::seekId, this, _1, _2));
    dev->addActionMapping(this, "SeekIndex",
                          bind(&OHPlaylist::seekIndex, this, _1, _2));
    dev->addActionMapping(this, "TransportState",
                          bind(&OHPlaylist::transportState, this, _1, _2));
    dev->addActionMapping(this, "Id",
                          bind(&OHPlaylist::id, this, _1, _2));
    dev->addActionMapping(this, "Read",
                          bind(&OHPlaylist::ohread, this, _1, _2));
    dev->addActionMapping(this, "ReadList",
                          bind(&OHPlaylist::readlist, this, _1, _2));
    dev->addActionMapping(this, "Insert",
                          bind(&OHPlaylist::insert, this, _1, _2));
    dev->addActionMapping(this, "DeleteId",
                          bind(&OHPlaylist::deleteId, this, _1, _2));
    dev->addActionMapping(this, "DeleteAll",
                          bind(&OHPlaylist::deleteAll, this, _1, _2));
    dev->addActionMapping(this, "TracksMax",
                          bind(&OHPlaylist::tracksMax, this, _1, _2));
    dev->addActionMapping(this, "IdArray",
                          bind(&OHPlaylist::idArray, this, _1, _2));
    dev->addActionMapping(this, "IdArrayChanged",
                          bind(&OHPlaylist::idArrayChanged, this, _1, _2));
    dev->addActionMapping(this, "ProtocolInfo",
                          bind(&OHPlaylist::protocolInfo, this, _1, _2));
    dev->m_mpdcli->consume(false);
}

static const int tracksmax = 16384;

// Yes inefficient. whatever...
static string makesint(int val)
{
    char cbuf[30];
    sprintf(cbuf, "%d", val);
    return string(cbuf);
}

static string mpdstatusToTransportState(MpdStatus::State st)
{
    string tstate;
    switch(st) {
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

// The data format for id lists is an array of msb 32b its ints
// encoded in base64...
static string translateIdArray(const vector<unsigned int>& in)
{
    string out1;
    for (vector<unsigned int>::const_iterator it = in.begin(); 
         it != in.end(); it++) {
        out1 += (unsigned char) (((*it) & 0xff000000) >> 24);
        out1 += (unsigned char) ( ((*it) & 0x00ff0000) >> 16);
        out1 += (unsigned char) ( ((*it) & 0x0000ff00) >> 8);
        out1 += (unsigned char) (  (*it) & 0x000000ff);
    }
    return base64_encode(out1);
}

string OHPlaylist::makeIdArray()
{
    string out;
    vector<unsigned int> vids;
    unordered_map<int, string> nmeta;
    bool ok = m_dev->m_mpdcli->getQueueIds(vids);
    if (ok) {
        out = translateIdArray(vids);
    }

    // Clear metadata cache: entries not in vids are not interesting
    for (auto& id : vids) {
        unordered_map<int, string>::iterator it1 = m_metacache.find(id);
        if (it1 != m_metacache.end())
            nmeta[id] = it1->second;
    }
    m_metacache = nmeta;

    return out;
}

bool OHPlaylist::makestate(unordered_map<string, string> &st)
{
    st.clear();

    const MpdStatus &mpds = m_dev->getMpdStatus();

    st["TransportState"] =  mpdstatusToTransportState(mpds.state);
    st["Repeat"] = makesint(mpds.rept);
    st["Shuffle"] = makesint(mpds.random);
    st["Id"] = makesint(mpds.songid);
    st["TracksMax"] = makesint(tracksmax);
    st["ProtocolInfo"] = upmpdProtocolInfo;
    st["IdArray"] = makeIdArray();

    return true;
}

bool OHPlaylist::getEventData(bool all, std::vector<std::string>& names, 
                            std::vector<std::string>& values)
{
    //LOGDEB("OHPlaylist::getEventData" << endl);

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

void OHPlaylist::maybeWakeUp(bool ok)
{
    if (ok && m_dev)
        m_dev->loopWakeup();
}

int OHPlaylist::play(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::play" << endl);
    bool ok = m_dev->m_mpdcli->play();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::pause(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::pause" << endl);
    bool ok = m_dev->m_mpdcli->pause(true);
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::stop(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::stop" << endl);
    bool ok = m_dev->m_mpdcli->stop();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::next(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::next" << endl);
    bool ok = m_dev->m_mpdcli->next();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::previous(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::previous" << endl);
    bool ok = m_dev->m_mpdcli->previous();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::setRepeat(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::setRepeat" << endl);
    bool onoff;
    bool ok = sc.getBool("Value", &onoff);
    if (ok) {
        ok = m_dev->m_mpdcli->repeat(onoff);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::repeat(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::repeat" << endl);
    const MpdStatus &mpds =  m_dev->getMpdStatus();
    data.addarg("Value", mpds.rept? "1" : "0");
    return UPNP_E_SUCCESS;
}

int OHPlaylist::setShuffle(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::setShuffle" << endl);
    bool onoff;
    bool ok = sc.getBool("Value", &onoff);
    if (ok) {
        // Note that mpd shuffle shuffles the playlist, which is different
        // from playing at random
        ok = m_dev->m_mpdcli->random(onoff);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::shuffle(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::shuffle" << endl);
    const MpdStatus &mpds =  m_dev->getMpdStatus();
    data.addarg("Value", mpds.random ? "1" : "0");
    return UPNP_E_SUCCESS;
}

int OHPlaylist::seekSecondAbsolute(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::seekSecondAbsolute" << endl);
    int seconds;
    bool ok = sc.getInt("Value", &seconds);
    if (ok) {
        ok = m_dev->m_mpdcli->seek(seconds);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::seekSecondRelative(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::seekSecondRelative" << endl);
    int seconds;
    bool ok = sc.getInt("Value", &seconds);
    if (ok) {
        const MpdStatus &mpds =  m_dev->getMpdStatus();
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

int OHPlaylist::transportState(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::transportState" << endl);
    const MpdStatus &mpds = m_dev->getMpdStatus();
    string tstate;
    switch(mpds.state) {
    case MpdStatus::MPDS_PLAY: 
        tstate = "Playing";
        break;
    case MpdStatus::MPDS_PAUSE: 
        tstate = "Paused";
        break;
    default:
        tstate = "Stopped";
    }
    data.addarg("TransportState", tstate);
    return UPNP_E_SUCCESS;
}

// Skip to track specified by Id
int OHPlaylist::seekId(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::seekId" << endl);
    int id;
    bool ok = sc.getInt("Value", &id);
    if (ok) {
        ok = m_dev->m_mpdcli->playId(id);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Skip to track with specified index 
int OHPlaylist::seekIndex(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::seekIndex" << endl);
    int pos;
    bool ok = sc.getInt("Value", &pos);
    if (ok) {
        ok = m_dev->m_mpdcli->play(pos);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Return current Id
int OHPlaylist::id(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::id" << endl);
    const MpdStatus &mpds = m_dev->getMpdStatus();
    data.addarg("Value", makesint(mpds.songid));
    return UPNP_E_SUCCESS;
}

// Report the uri and metadata for a given track id. 
// Returns a 800 fault code if the given id is not in the playlist. 
int OHPlaylist::ohread(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::ohread" << endl);
    int id;
    bool ok = sc.getInt("Value", &id);
    UpSong song;
    if (ok) {
        ok = m_dev->m_mpdcli->statSong(song, id, true);
    }
    if (ok) {
        unordered_map<int, string>::iterator mit = m_metacache.find(id);
        string metadata;
        if (mit != m_metacache.end()) {
            metadata = xmlquote(mit->second);
        } else {
            metadata = xmlquote(didlmake(song));
        }
        data.addarg("Uri", song.uri);
        data.addarg("Metadata", metadata);
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
// Any ids not in the playlist are ignored. 
int OHPlaylist::readlist(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::readlist" << endl);
    string sids;
    bool ok = sc.getString("IdList", &sids);
    vector<string> ids;
    string out("<TrackList>");
    if (ok) {
        stringToTokens(sids, ids);
        for (vector<string>::iterator it = ids.begin(); it != ids.end(); it++) {
            int id = atoi(it->c_str());
            UpSong song;
            if (!m_dev->m_mpdcli->statSong(song, id, true))
                continue;
            unordered_map<int, string>::iterator mit = m_metacache.find(id);
            string metadata;
            if (mit != m_metacache.end()) {
                metadata = xmlquote(mit->second);
            } else {
                metadata = xmlquote(didlmake(song));
            }
            out += "<Entry><Id>";
            out += it->c_str();
            out += "</Id><Uri>";
            out += song.uri;
            out += "</Uri><Metadata>";
            out += metadata;
            out += "</Metadata></Entry>";
        }
        out += "</TrackList>";
        data.addarg("TrackList", out);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

// Adds the given uri and metadata as a new track to the playlist. 
// Set the AfterId argument to 0 to insert a track at the start of the
// playlist.
// Reports a 800 fault code if AfterId is not 0 and doesn’t appear in
// the playlist.
// Reports a 801 fault code if the playlist is full (i.e. already
// contains TracksMax tracks).
int OHPlaylist::insert(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::insert" << endl);
    int afterid;
    string uri, metadata;
    bool ok = sc.getInt("AfterId", &afterid);
    ok = ok && sc.getString("Uri", &uri);
    if (ok)
        ok = ok && sc.getString("Metadata", &metadata);

    LOGDEB("OHPlaylist::insert: afterid " << afterid << " Uri " <<
           uri << " Metadata " << metadata << endl);
    if (ok) {
        int id = m_dev->m_mpdcli->insertAfterId(uri, afterid);
        if ((ok = (id != -1)))
            m_metacache[id] = metadata;
    }
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::deleteId(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::deleteId" << endl);
    int id;
    bool ok = sc.getInt("Value", &id);
    if (ok) {
        ok = m_dev->m_mpdcli->deleteId(id);
        maybeWakeUp(ok);
    }
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::deleteAll(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::deleteAll" << endl);
    bool ok = m_dev->m_mpdcli->clearQueue();
    maybeWakeUp(ok);
    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::tracksMax(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::tracksMax" << endl);
    data.addarg("Value", makesint(tracksmax));
    return UPNP_E_SUCCESS;
}

// Returns current list of id as array of big endian 32bits integers,
// base-64-encoded. 
int OHPlaylist::idArray(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::idArray" << endl);
    data.addarg("Token", "0");
    data.addarg("Array", makeIdArray());
    return UPNP_E_SUCCESS;
}

// Check if id array changed since last call (which returned a gen token)
int OHPlaylist::idArrayChanged(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::idArrayChanged" << endl);
    bool ok = false;

    data.addarg("Token", "0");
    // Bool indicating if array changed
    data.addarg("Value", "0");

    return ok ? UPNP_E_SUCCESS : UPNP_E_INTERNAL_ERROR;
}

int OHPlaylist::protocolInfo(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("OHPlaylist::protocolInfo" << endl);
    data.addarg("Value", upmpdProtocolInfo);
    return UPNP_E_SUCCESS;
}
