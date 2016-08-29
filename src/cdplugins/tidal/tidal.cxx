/* Copyright (C) 2016 J.F.Dockes
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

#include "tidal.hxx"

#define LOGGER_LOCAL_LOGINC 3

#include <fcntl.h>

#include <string>
#include <vector>
#include <sstream>
#include <string.h>
#include <upnp/upnp.h>
#include <microhttpd.h>
extern "C" {
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}


#include "cmdtalk.h"
#include "pathut.h"
#include "smallut.h"
#include "log.hxx"
#include "json.hpp"
#include "main.hxx"
#include "conftree.h"

using namespace std;
using namespace std::placeholders;
using json = nlohmann::json;
using namespace UPnPProvider;

class StreamHandle {
public:
    StreamHandle(Tidal::Internal *plg) {
    }
    ~StreamHandle() {
        clear();
    }
    void clear() {
        plg = 0;
        path.clear();
        media_url.clear();
        len = 0;
        opentime = 0;
    }
    
    Tidal::Internal *plg;
    AVIOContext *avio;
    string path;
    string media_url;
    long long len;
    time_t opentime;
};

class Tidal::Internal {
public:
    Internal(Tidal *tidal, const vector<string>& pth, const string& hst,
             int prt, const string& pp)
	: plg(tidal), path(pth), host(hst), port(prt), pathprefix(pp), kbs(0),
          laststream(this),
          mhd(0) { }

    bool maybeStartCmd(const string&);
    string get_media_url(const std::string& path);
    // This also sets the kbs
    string get_mimetype(const std::string& path);

    Tidal *plg;
    CmdTalk cmd;
    vector<string> path;
    // Host and port for the URLs we generate.
    string host;
    int port;
    // path prefix (this is used by upmpdcli that a gets is for us).
    string pathprefix;
    // mimetype is a constant for a given session, depend on quality
    // choice only. Initialized once
    string mimetype;
    // kilobits/s
    int kbs;
    
    // Cached uri translation and stream: set in getinfo() and reused
    // in open() (used with miniserver/vdir)
    StreamHandle laststream;
    // When using microhttpd
    struct MHD_Daemon *mhd;
};


// Microhttpd connection handler. We re-build the complete url + query
// string (&trackid=value), use this to retrieve a Tidal URL, and
// redirect to it (HTTP). A previous version handled rtmp streams, and
// had to read them. Look up the history if you need the code again
// (the apparition of RTMP streams was apparently linked to the use of
// a different API key).
static int answer_to_connection(void *cls, struct MHD_Connection *connection, 
                                const char *url, 
                                const char *method, const char *version, 
                                const char *upload_data, 
                                size_t *upload_data_size, void **con_cls)
{
    LOGDEB("answer_to_connection: url " << url << " method " << method << 
           " version " << version << endl);
    Tidal::Internal *me = (Tidal::Internal*)cls;
    
    static int aptr;
    if (&aptr != *con_cls) {
        /* do not respond on first call */
        *con_cls = &aptr;
        return MHD_YES;
    }

    // Rebuild URL + query
    string path(url);
    const char* stid =
        MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND,
                                    "trackId");
    if (!stid || !*stid) {
        LOGERR("answer_to_connection: no trackId in args\n");
        return MHD_NO;
    }
    path += string("?version=1&trackId=") + stid;

    // Translate to Tidal URL
    string media_url = me->get_media_url(path);
    if (media_url.empty()) {
        LOGERR("answer_to_connection: no media_uri for: " << url << endl);
        return MHD_NO;
    }

    if (media_url.find("http") == 0) {
        LOGDEB("Tidal: redirecting to " << media_url << endl);

        static char data[] = "<html><body></body></html>";
        struct MHD_Response *response =
            MHD_create_response_from_buffer(strlen(data), data,
                                            MHD_RESPMEM_PERSISTENT);
        if (response == NULL) {
            LOGERR("answer_to_connection: could not create response" << endl);
            return MHD_NO;
        }
        MHD_add_response_header (response, "Location", media_url.c_str());
        int ret = MHD_queue_response(connection, 302, response);
        MHD_destroy_response(response);
        return ret;
    } else {
        LOGERR("Tidal: got non-http URL !: " << media_url << endl);
        LOGERR("Tidal:   the code for handling these is gone !\n");
        LOGERR("    will have to fetch it from git history\n");
        return MHD_NO;
    } 
}

static int accept_policy(void *, const struct sockaddr* sa, socklen_t addrlen)
{
    return MHD_YES;
}

// Called once for starting the Python program and do other initialization.
bool Tidal::Internal::maybeStartCmd(const string& who)
{
    LOGDEB("Tidal::maybeStartCmd for: " << who << endl);
    
    if (cmd.running()) {
        return true;
    }

    ConfSimple *conf = plg->m_services->getconfig(plg);
    string tidalquality;
    if (!conf || !conf->get("tidalquality", tidalquality)) {
        LOGERR("Tidal: can't get parameter 'tidalquality' from config\n");
        return false;
    }
    
    port = 49149;
    string sport;
    if (conf->get("tidalmicrohttpport", sport)) {
        port = atoi(sport.c_str());
    }
    mhd = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        //MHD_USE_SELECT_INTERNALLY, 
        port, 
        /* Accept policy callback and arg */
        accept_policy, NULL, 
        /* handler and arg */
        &answer_to_connection, this, 
        MHD_OPTION_END);
    if (nullptr == mhd) {
        LOGERR("Tidal: MHD_start_daemon failed\n");
        return false;
    }
        
    string pythonpath = string("PYTHONPATH=") +
        path_cat(g_datadir, "cdplugins/pycommon");
    string configname = string("UPMPD_CONFIG=") + g_configfilename;
    stringstream ss;
    ss << host << ":" << port;
    string hostport = string("UPMPD_HTTPHOSTPORT=") + ss.str();
    string pp = string("UPMPD_PATHPREFIX=") + pathprefix;
    if (!cmd.startCmd("tidal.py", {/*args*/},
                      /* env */ {pythonpath, configname, hostport, pp},
                      /* exec path */ path)) {
        LOGERR("Tidal::maybeStartCmd: " << who << " startCmd failed\n");
        return false;
    }
    return true;
}

string Tidal::Internal::get_mimetype(const std::string& path)
{
    if (!maybeStartCmd("get_mimetype")) {
	return string();
    }
    if (mimetype.empty()) {
	unordered_map<string, string> res;
	if (!cmd.callproc("mimetype", {{"path", path}}, res)) {
	    LOGERR("Tidal::get_mimetype: slave failure\n");
	    return string();
	}

	auto it = res.find("mimetype");
	if (it == res.end()) {
	    LOGERR("Tidal::get_mimetype: no mimetype in result\n");
	    return string();
	}
	mimetype = it->second;
	it = res.find("kbs");
	if (it != res.end()) {
            kbs = atoi(it->second.c_str());
	}
	LOGDEB("Tidal: got mimetype [" << mimetype << "]\n");
    }
    return mimetype;
}

// Translate our http URL (based on the trackid), to an actual
// temporary TIDAL one, which will be an HTTP URL pointing to either
// an AAC or a FLAC stream.
// Older versions of this module handled AAC FLV transported over
// RTMP, apparently because of the use of a different API key. Look up
// the git history if you need this again.
// The Python code calls tidal.com to translate the trackid to a temp
// URL. We cache the result for a few seconds to avoid multiple calls
// to tidal.
string Tidal::Internal::get_media_url(const std::string& path)
{
    LOGDEB("Tidal::get_media_url: " << path << endl);
    if (!maybeStartCmd("get_media_url")) {
	return string();
    }
    time_t now = time(0);
    if (laststream.path.compare(path) || (now - laststream.opentime > 10)) {
	unordered_map<string, string> res;
	if (!cmd.callproc("trackuri", {{"path", path}}, res)) {
	    LOGERR("Tidal::get_media_url: slave failure\n");
	    return string();
	}

	auto it = res.find("media_url");
	if (it == res.end()) {
	    LOGERR("Tidal::get_media_url: no media url in result\n");
	    return string();
	}
        laststream.clear();
        laststream.path = path;
        laststream.media_url = it->second;
        laststream.opentime = now;
    }

    LOGDEB("Tidal: got media url [" << laststream.media_url << "]\n");
    return laststream.media_url;
}


Tidal::Tidal(const std::string& name, CDPluginServices *services)
    : CDPlugin(name, services)
{
    m = new Internal(this, services->getexecpath(this),
                     services->getupnpaddr(this),
                     services->getupnpport(this),
                     services->getpathprefix(this));
}

Tidal::~Tidal()
{
    delete m;
}

static int resultToEntries(const string& encoded, int stidx, int cnt,
			   std::vector<UpSong>& entries)
{
    auto decoded = json::parse(encoded);
    LOGDEB("Tidal::results: got " << decoded.size() << " entries\n");
    LOGDEB1("Tidal::results: undecoded json: " << decoded.dump() << endl);

    for (unsigned int i = stidx; i < decoded.size(); i++) {
	if (--cnt < 0) {
	    break;
	}
	UpSong song;
	// tp is container ("ct") or item ("it")
	auto it1 = decoded[i].find("tp");
	if (it1 == decoded[i].end()) {
	    LOGERR("Tidal::browse: no type in entry\n");
	    continue;
	}
	string stp = it1.value();
	
#define JSONTOUPS(fld, nm)						\
	it1 = decoded[i].find(#nm);					\
	if (it1 != decoded[i].end()) {					\
	    /*LOGDEB("song." #fld " = " << it1.value() << endl);*/	\
	    song.fld = it1.value();					\
	}
	
	if (!stp.compare("ct")) {
	    song.iscontainer = true;
	} else	if (!stp.compare("it")) {
	    song.iscontainer = false;
	    JSONTOUPS(uri, uri);
	    JSONTOUPS(artist, dc:creator);
	    JSONTOUPS(artist, upnp:artist);
	    JSONTOUPS(genre, upnp:genre);
	    JSONTOUPS(tracknum, upnp:originalTrackNumber);
	    JSONTOUPS(artUri, upnp:albumArtURI);
	    JSONTOUPS(duration_secs, duration);
	} else {
	    LOGERR("Tidal::browse: bad type in entry: " << it1.value() << endl);
	    continue;
	}
	JSONTOUPS(id, id);
	JSONTOUPS(parentid, pid);
	JSONTOUPS(title, tt);
	entries.push_back(song);
    }
    // We return the total match size, the count of actually returned
    // entries can be obtained from the vector
    return decoded.size();
}

int Tidal::browse(const std::string& objid, int stidx, int cnt,
		  std::vector<UpSong>& entries,
		  const std::vector<std::string>& sortcrits,
		  BrowseFlag flg)
{
    LOGDEB("Tidal::browse\n");
    if (!m->maybeStartCmd("browse")) {
	return -1;
    }
    string sbflg;
    switch (flg) {
    case CDPlugin::BFMeta:
        sbflg = "meta";
        break;
    case CDPlugin::BFChildren:
    default:
        sbflg = "children";
        break;
    }

    unordered_map<string, string> res;
    if (!m->cmd.callproc("browse", {{"objid", objid}, {"flag", sbflg}}, res)) {
	LOGERR("Tidal::browse: slave failure\n");
	return -1;
    }

    auto it = res.find("entries");
    if (it == res.end()) {
	LOGERR("Tidal::browse: no entries returned\n");
	return -1;
    }
    return resultToEntries(it->second, stidx, cnt, entries);
}


int Tidal::search(const string& ctid, int stidx, int cnt,
		  const string& searchstr,
		  vector<UpSong>& entries,
		  const vector<string>& sortcrits)
{
    LOGDEB("Tidal::search\n");
    if (!m->maybeStartCmd("search")) {
	return -1;
    }

    // We only accept field xx value as search criteria
    vector<string> vs;
    stringToStrings(searchstr, vs);
    LOGDEB("Tidal::search:search string split->" << vs.size() << " pieces\n");
    if (vs.size() != 3) {
	LOGERR("Tidal::search: bad search string: [" << searchstr << "]\n");
	return -1;
    }
    const string& upnpproperty = vs[0];
    string tidalfield;
    if (!upnpproperty.compare("upnp:artist") ||
	!upnpproperty.compare("dc:author")) {
	tidalfield = "artist";
    } else if (!upnpproperty.compare("upnp:album")) {
	tidalfield = "album";
    } else if (!upnpproperty.compare("dc:title")) {
	tidalfield = "track";
    } else {
	LOGERR("Tidal::search: bad property: [" << upnpproperty << "]\n");
	return -1;
    }
	
    unordered_map<string, string> res;
    if (!m->cmd.callproc("search", {
		{"objid", ctid},
		{"field", tidalfield},
		{"value", vs[2]} },  res)) {
	LOGERR("Tidal::search: slave failure\n");
	return -1;
    }

    auto it = res.find("entries");
    if (it == res.end()) {
	LOGERR("Tidal::search: no entries returned\n");
	return -1;
    }
    return resultToEntries(it->second, stidx, cnt, entries);
}
