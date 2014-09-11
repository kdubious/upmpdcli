/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <arpa/inet.h>

#include <string>
#include <vector>

using namespace std;

#include <upnp/upnp.h>

#include "libupnpp/base64.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/expatmm.hxx"
#include "libupnpp/control/ohplaylist.hxx"

using namespace UPnPP;

namespace UPnPClient {

const string OHPlaylist::SType("urn:av-openhome-org:service:Playlist:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHPlaylist::isOHPlService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}


int OHPlaylist::play()
{
    return runTrivialAction("Play");
}
int OHPlaylist::pause()
{
    return runTrivialAction("Pause");
}
int OHPlaylist::stop()
{
    return runTrivialAction("Stop");
}
int OHPlaylist::next()
{
    return runTrivialAction("Next");
}
int OHPlaylist::previous()
{
    return runTrivialAction("Previous");
}
int OHPlaylist::setRepeat(bool onoff)
{
    return runSimpleAction("SetRepeat", "Value", onoff);
}
int OHPlaylist::repeat(bool *on)
{
    return runSimpleGet("Repeat", "Value", on);
}
int OHPlaylist::setShuffle(bool onoff)
{
    return runSimpleAction("SetShuffle", "Value", onoff);
}
int OHPlaylist::shuffle(bool *on)
{
    return runSimpleGet("Shuffle", "Value", on);
}
int OHPlaylist::seekSecondAbsolute(int value)
{
    return runSimpleAction("SeekSecondAbsolute", "Value", value);
}
int OHPlaylist::seekSecondRelative(int value)
{
    return runSimpleAction("SeekSecondRelative", "Value", value);
}
int OHPlaylist::seekId(int value)
{
    return runSimpleAction("SeekId", "Value", value);
}
int OHPlaylist::seekIndex(int value)
{
    return runSimpleAction("SeekIndex", "Value", value);
}

int OHPlaylist::transportState(TPState* tpp)
{
    std::string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value)))
        return ret;
    if (!value.compare("Buffering")) {
        *tpp = TPS_Buffering;
        return 0;
    } else if (!value.compare("Paused")) {
        *tpp = TPS_Paused;
        return 0;
    } else if (!value.compare("Playing")) {
        *tpp = TPS_Playing;
        return 0;
    } else if (!value.compare("Stopped")) {
        *tpp = TPS_Stopped;
        return 0;
    }
    *tpp = TPS_Unknown;
    return UPNP_E_BAD_RESPONSE;
}

int OHPlaylist::id(int *value)
{
    return runSimpleGet("Id", "Value", value);
}

int OHPlaylist::read(int id, std::string* urip, UPnPDirObject *dirent)
{
    SoapEncodeInput args(m_serviceType, "Read");
    args("Id", SoapHelp::i2s(id));
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", urip)) {
        LOGERR("OHPlaylist::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHPlaylist::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    didl = SoapHelp::xmlUnquote(didl);

    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHPlaylist::Read: didl parse failed: " << didl << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHPlaylist::Read: " << dir.m_items.size() << " in response!" <<
               endl);
        return UPNP_E_BAD_RESPONSE;
    }
    *dirent = dir.m_items[0];
    return 0;
}

// Tracklist format
// <TrackList>
//   <Entry>
//     <Id>10</Id>
//     <Uri>http://blabla</Uri>
//     <Metadata>(xmlencoded didl)</Metadata>
//   </Entry>
// </TrackList>


class OHTrackListParser : public inputRefXMLParser {
public:
    OHTrackListParser(const string& input, 
                      vector<OHPlaylist::TrackListEntry>* vp)
        : inputRefXMLParser(input), m_v(vp)
        {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **) {
        m_path.push_back(name);
    }
    virtual void EndElement(const XML_Char *name) {
        if (!strcmp(name, "Entry")) {
            UPnPDirContent dir;
            if (!dir.parse(SoapHelp::xmlUnquote(m_tdidl))) {
                LOGERR("OHPlaylist::ReadList: didl parse failed: " 
                       << m_tdidl << endl);
                return;
            }
            if (dir.m_items.size() != 1) {
                LOGERR("OHPlaylist::ReadList: " << dir.m_items.size() 
                       << " in response!" << endl);
                return;
            }
            m_tt.dirent = dir.m_items[0];
            m_v->push_back(m_tt);
            m_tt.clear();
            m_tdidl.clear();
        }
        m_path.pop_back();
    }
    virtual void CharacterData(const XML_Char *s, int len) {
        if (s == 0 || *s == 0)
            return;
        string str(s, len);
        if (!m_path.back().compare("Id"))
            m_tt.id = atoi(str.c_str());
        else if (!m_path.back().compare("Uri"))
            m_tt.url = str;
        else if (!m_path.back().compare("Metadata"))
            m_tdidl += str;
    }

private:
    vector<OHPlaylist::TrackListEntry>* m_v;
    std::vector<std::string> m_path;
    OHPlaylist::TrackListEntry m_tt;
    string m_tdidl;
};

int OHPlaylist::readList(const std::vector<int>& ids, 
                         vector<TrackListEntry>* entsp)
{
    string idsparam;
    for (auto it = ids.begin(); it != ids.end(); it++) {
        idsparam += SoapHelp::i2s(*it) + " ";
    }
    entsp->clear();

    SoapEncodeInput args(m_serviceType, "ReadList");
    args("IdList", idsparam);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string xml;
    if (!data.get("TrackList", &xml)) {
        LOGERR("OHPlaylist::readlist: missing TrackList in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    OHTrackListParser mparser(xml, entsp);
    if (!mparser.Parse())
        return UPNP_E_BAD_RESPONSE;
    return 0;
}

int OHPlaylist::insert(int afterid, const string& uri, const string& didl, 
                       int *nid)
{
    SoapEncodeInput args(m_serviceType, "Insert");
    args("AfterId", SoapHelp::i2s(afterid))
        ("Uri", uri)
        ("Metadata", didl);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("NewId", nid)) {
        LOGERR("OHPlaylist::insert: missing Newid in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}


int OHPlaylist::deleteId(int value)
{
    return runSimpleAction("DeleteId", "Value", value);
}
int OHPlaylist::deleteAll()
{
    return runTrivialAction("DeleteAll");
}
int OHPlaylist::tracksMax(int *valuep)
{
    return runSimpleGet("TracksMax", "Value", valuep);
}

int OHPlaylist::idArray(vector<int> *ids, int *tokp)
{
    SoapEncodeInput args(m_serviceType, "IdArray");
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Token", tokp)) {
        LOGERR("OHPlaylist::insert: missing Token in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    string arraydata;
    if (!data.get("Array", &arraydata)) {
        LOGERR("OHPlaylist::insert: missing Array in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    arraydata = base64_decode(arraydata);
    const char *cp = arraydata.c_str();
    while (cp - arraydata.c_str() < int(arraydata.size())) {
        unsigned int *ip = (unsigned int *)cp;
        ids->push_back(ntohl(*ip));
        cp += 4;
    }
    return 0;
}

int OHPlaylist::idArrayChanged(int token, bool *changed)
{
    SoapEncodeInput args(m_serviceType, "IdArrayChanged");
    args("Token", SoapHelp::i2s(token));
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", changed)) {
        LOGERR("OHPlaylist::idArrayChanged: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHPlaylist::protocolInfo(std::string *proto)
{
    SoapEncodeInput args(m_serviceType, "ProtocolInfo");
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHPlaylist::protocolInfo: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

} // End namespace UPnPClient

