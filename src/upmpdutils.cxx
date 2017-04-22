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

//
// This file has a number of mostly uninteresting and badly
// implemented small utility functions. This is a bit ugly, but I am
// not linking to Qt or glib just to get path-concatenating
// functions...

#include "upmpdutils.hxx"

#include <errno.h>                      // for errno
#include <fcntl.h>                      // for open, O_RDONLY, O_CREAT, etc
#include <math.h>                       // for exp10, floor, log10, sqrt
#include <pwd.h>                        // for getpwnam, getpwuid, passwd
#include <regex.h>                      // for regmatch_t, regfree, etc
#include <stdio.h>                      // for sprintf
#include <stdlib.h>                     // for getenv, strtol
#include <string.h>                     // for strerror, strerror_r
#include <sys/file.h>                   // for flock, LOCK_EX, LOCK_NB
#include <sys/stat.h>                   // for fstat, mkdir, stat
#include <unistd.h>                     // for close, pid_t, ftruncate, etc

#ifndef O_STREAMING
#define O_STREAMING 0
#endif
#include <fstream>                      // for operator<<, basic_ostream, etc
#include <sstream>                      // for ostringstream
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/log.hxx"             // for LOGERR
#include "libupnpp/soaphelp.hxx"        // for xmlQuote
#include "libupnpp/upnpavutils.hxx"     // for upnpduration
#include "libupnpp/control/cdircontent.hxx"

#include "mpdcli.hxx"                   // for UpSong
#include "smallut.h"

using namespace std;
using namespace UPnPP;
using namespace UPnPClient;

// Translate 0-100% MPD volume to UPnP VolumeDB: we do db upnp-encoded
// values from -10240 (0%) to 0 (100%)
int percentodbvalue(int value)
{
    int dbvalue;
    if (value == 0) {
        dbvalue = -10240;
    } else {
        float ratio = float(value) * value / 10000.0;
        float db = 10 * log10(ratio);
        dbvalue = int(256 * db);
    }
    return dbvalue;
}

#ifdef __APPLE__
#define exp10 __exp10
#endif
#ifdef __UCLIBC__
/* 10^x = 10^(log e^x) = (e^x)^log10 = e^(x * log 10) */
#define exp10(x) (exp((x) * log(10)))
#endif /* __UCLIBC__ */

// Translate VolumeDB to MPD 0-100
int dbvaluetopercent(int dbvalue)
{
    float db = float(dbvalue) / 256.0;
    float vol = exp10(db / 10);
    int percent = floor(sqrt(vol * 10000.0));
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    return percent;
}

// Get from ssl unordered_map, return empty string for non-existing
// key (so this only works for data where this behaviour makes sense).
const string& mapget(const unordered_map<string, string>& im, const string& k)
{
    static string ns; // null string
    unordered_map<string, string>::const_iterator it = im.find(k);
    if (it == im.end()) {
        return ns;
    } else {
        return it->second;
    }
}

unordered_map<string, string>
diffmaps(const unordered_map<string, string>& old,
         const unordered_map<string, string>& newer)
{
    unordered_map<string, string>  out;

    for (unordered_map<string, string>::const_iterator it = newer.begin();
            it != newer.end(); it++) {
        unordered_map<string, string>::const_iterator ito = old.find(it->first);
        if (ito == old.end() || ito->second.compare(it->second)) {
            out[it->first] = it->second;
        }
    }
    return out;
}


#define UPNPXML(FLD, TAG)                                               \
    if (!FLD.empty()) {                                                 \
        ss << "<" #TAG ">" << SoapHelp::xmlQuote(FLD) << "</" #TAG ">"; \
    }
#define UPNPXMLD(FLD, TAG, DEF)                                         \
    if (!FLD.empty()) {                                                 \
        ss << "<" #TAG ">" << SoapHelp::xmlQuote(FLD) << "</" #TAG ">"; \
    } else {                                                            \
        ss << "<" #TAG ">" << SoapHelp::xmlQuote(DEF) << "</" #TAG ">"; \
    }

string UpSong::didl()
{
    ostringstream ss;
    string typetag;
    if (iscontainer) {
	typetag = "container";
    } else {
	typetag = "item";
    }
    ss << "<" << typetag << " id=\"" << id << "\" parentID=\"" <<
	parentid << "\" restricted=\"1\" searchable=\"" <<
	(searchable ? string("1") : string("0")) << "\">" <<
	"<dc:title>" << SoapHelp::xmlQuote(title) << "</dc:title>";

    if (iscontainer) {
        UPNPXMLD(upnpClass, upnp:class, "object.container");
        // tracknum is reused for annotations for containers
        ss << (tracknum.empty() ? string() :
               string("<upnp:userAnnotation>" + SoapHelp::xmlQuote(tracknum) +
		    "</upnp:userAnnotation>"));
	    
    } else {
        UPNPXMLD(upnpClass, upnp:class, "object.item.audioItem.musicTrack");
	UPNPXML(genre, upnp:genre);
	UPNPXML(album, upnp:album);
	UPNPXML(tracknum, upnp:originalTrackNumber);

	ss << "<res " <<
            "duration=\"" << upnpduration(duration_secs * 1000)  << "\" " <<
            "size=\"" << lltodecstr(size)                        << "\" " <<
            "bitrate=\"" << SoapHelp::i2s(bitrate)               << "\" " <<
	    "sampleFrequency=\"" << SoapHelp::i2s(samplefreq)    << "\" " <<
            "nrAudioChannels=\"" << SoapHelp::i2s(channels)      << "\" " <<
	    "protocolInfo=\"http-get:*:" << mime << ":* "        << "\" >"<<
            SoapHelp::xmlQuote(uri) <<
            "</res>";
    }
    UPNPXML(artist, dc:creator);
    UPNPXML(artist, upnp:artist);
    UPNPXML(date, dc:date);
    UPNPXML(artUri, upnp:albumArtURI);
    ss << "</" << typetag << ">";
    LOGDEB1("UpSong::didl(): " << ss.str() << endl);
    return ss.str();
}

const string& headDIDL()
{
    static const string head(
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
	"<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
	"xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
	"xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
	"xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">");
    return head;
}

const string& tailDIDL()
{
    static const string tail("</DIDL-Lite>");
    return tail;
}
    
string wrapDIDL(const std::string& data)
{
    return headDIDL() + data + tailDIDL();
}

// Bogus didl fragment maker. We probably don't need a full-blown XML
// helper here
string didlmake(const UpSong& song)
{
    ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
       "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
       "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
       "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
       "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">"
       << "<item restricted=\"1\">";
    ss << "<orig>mpd</orig>";
    {
        const string& val = song.title;
        ss << "<dc:title>" << SoapHelp::xmlQuote(val) << "</dc:title>";
    }

    // TBD Playlists etc?
    ss << "<upnp:class>object.item.audioItem.musicTrack</upnp:class>";

    {
        const string& val = song.artist;
        if (!val.empty()) {
            string a = SoapHelp::xmlQuote(val);
            ss << "<dc:creator>" << a << "</dc:creator>" <<
               "<upnp:artist>" << a << "</upnp:artist>";
        }
    }

    {
        const string& val = song.album;
        if (!val.empty()) {
            ss << "<upnp:album>" << SoapHelp::xmlQuote(val) << "</upnp:album>";
        }
    }

    {
        const string& val = song.genre;
        if (!val.empty()) {
            ss << "<upnp:genre>" << SoapHelp::xmlQuote(val) << "</upnp:genre>";
        }
    }

    {
        string val = song.tracknum;
        // MPD may return something like xx/yy
        string::size_type spos = val.find("/");
        if (spos != string::npos) {
            val = val.substr(0, spos);
        }
        if (!val.empty()) {
            ss << "<upnp:originalTrackNumber>" << val <<
               "</upnp:originalTrackNumber>";
        }
    }

    {
        const string& val = song.artUri;
        if (!val.empty()) {
            ss << "<upnp:albumArtURI>" << SoapHelp::xmlQuote(val) <<
               "</upnp:albumArtURI>";
        }
    }

    // TBD: the res element normally has size, sampleFrequency,
    // nrAudioChannels and protocolInfo attributes, which are bogus
    // for the moment. partly because MPD does not supply them.  And
    // mostly everything is bogus if next is set...

    ss << "<res " << "duration=\"" << upnpduration(song.duration_secs * 1000)
       << "\" "
       // Bitrate keeps changing for VBRs and forces events. Keeping
       // it out for now.
       //       << "bitrate=\"" << mpds.kbrate << "\" "
       << "sampleFrequency=\"44100\" nrAudioChannels=\"2\" "
       << "protocolInfo=\"http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000\""
       << ">"
       << SoapHelp::xmlQuote(song.uri)
       << "</res>"
       << "</item></DIDL-Lite>";
    return ss.str();
}

bool dirObjToUpSong(const UPnPDirObject& dobj, UpSong *ups)
{
    ups->artist = dobj.getprop("upnp:artist");
    ups->album = dobj.getprop("upnp:album");
    ups->title = dobj.m_title;
    string stmp;
    dobj.getrprop(0, "duration", stmp);
    if (!stmp.empty()) {
        ups->duration_secs = upnpdurationtos(stmp);
    } else {
        ups->duration_secs = 0;
    }
    ups->tracknum = dobj.getprop("upnp:originalTrackNumber");
    return true;
}

bool uMetaToUpSong(const string& metadata, UpSong *ups)
{
    if (ups == 0) {
        return false;
    }

    UPnPDirContent dirc;
    if (!dirc.parse(metadata) || dirc.m_items.size() == 0) {
        return false;
    }
    return dirObjToUpSong(*dirc.m_items.begin(), ups);
}
    
// Substitute regular expression
// The c++11 regex package does not seem really ready from prime time
// (Tried on gcc + libstdc++ 4.7.2-5 on Debian, with little
// joy). So...:
string regsub1(const string& sexp, const string& input, const string& repl)
{
    regex_t expr;
    int err;
    const int ERRSIZE = 200;
    char errbuf[ERRSIZE + 1];
    regmatch_t pmatch[10];

    if ((err = regcomp(&expr, sexp.c_str(), REG_EXTENDED))) {
        regerror(err, &expr, errbuf, ERRSIZE);
        LOGERR("upmpd: regsub1: regcomp() failed: " << errbuf << endl);
        return string();
    }

    if ((err = regexec(&expr, input.c_str(), 10, pmatch, 0))) {
        // regerror(err, &expr, errbuf, ERRSIZE);
        //LOGDEB("upmpd: regsub1: regexec(" << sexp << ") failed: "
        //<<  errbuf << endl);
        regfree(&expr);
        return input;
    }
    if (pmatch[0].rm_so == -1) {
        // No match
        regfree(&expr);
        return input;
    }
    string out = input.substr(0, pmatch[0].rm_so);
    out += repl;
    out += input.substr(pmatch[0].rm_eo);
    regfree(&expr);
    return out;
}
