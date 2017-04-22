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
#include "config.h"

#include "plgwithslave.hxx"

#include <fcntl.h>

#include <string>
#include <vector>
#include <sstream>
#include <string.h>
#include <upnp/upnp.h>
#include <microhttpd.h>
#include <json/json.h>

#include "cmdtalk.h"
#include "pathut.h"
#include "smallut.h"
#include "libupnpp/log.hxx"
#include "main.hxx"
#include "conftree.h"

using namespace std;
using namespace std::placeholders;
//using json = nlohmann::json;
using namespace UPnPProvider;

class StreamHandle {
public:
    StreamHandle(PlgWithSlave::Internal *plg) {
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
    
    PlgWithSlave::Internal *plg;
    string path;
    string media_url;
    long long len;
    time_t opentime;
};

class PlgWithSlave::Internal {
public:
    Internal(PlgWithSlave *_plg, const string& exe, const string& hst,
             int prt, const string& pp)
        : plg(_plg), exepath(exe), upnphost(hst), upnpport(prt), pathprefix(pp), 
          laststream(this) {
    }

    bool maybeStartCmd();

    PlgWithSlave *plg;
    CmdTalk cmd;
    string exepath;
    // Upnp Host and port. This would only be used to generate URLsif
    // we were using the libupnp miniserver. We currently use
    // microhttp because it can do redirects
    string upnphost;
    int upnpport;
    // path prefix (this is used by upmpdcli that gets it for us).
    string pathprefix;
    
    // Cached uri translation
    StreamHandle laststream;
};

// microhttpd daemon handle. There is only one of these, and one port, we find
// the right plugin by looking at the url path.
static struct MHD_Daemon *mhd;

// Microhttpd connection handler. We re-build the complete url + query
// string (&trackid=value), use this to retrieve a service URL
// (tidal/qobuz...), and redirect to it (HTTP). A previous version
// handled rtmp streams, and had to read them. Look up the history if
// you need the code again (the apparition of RTMP streams was
// apparently linked to the use of a different API key).
static int answer_to_connection(void *cls, struct MHD_Connection *connection, 
                                const char *url, 
                                const char *method, const char *version, 
                                const char *upload_data, 
                                size_t *upload_data_size, void **con_cls)
{
    static int aptr;
    if (&aptr != *con_cls) {
        /* do not respond on first call */
        *con_cls = &aptr;
        return MHD_YES;
    }

    LOGDEB("answer_to_connection: url " << url << " method " << method << 
           " version " << version << endl);

    // The 'plgi' here is just whatever plugin started up the httpd task
    // We just use it to find the appropriate plugin for this path,
    // and then dispatch the request.
    PlgWithSlave::Internal *plgi = (PlgWithSlave::Internal*)cls;
    PlgWithSlave *realplg =
        dynamic_cast<PlgWithSlave*>(plgi->plg->m_services->getpluginforpath(url));
    if (nullptr == realplg) {
        LOGERR("answer_to_connection: no plugin for path [" << url << endl);
        return MHD_NO;
    }

    // We may need one day to subclass PlgWithSlave to implement a
    // plugin-specific method. For now, existing plugins have
    // compatible python code, and we can keep one c++ method.
    // get_media_url() would also need changing because it is in
    // Internal: either make it generic or move to subclass.
    //return realplg->answer_to_connection(connection, url, method, version,
    //                               upload_data, upload_data_size, con_cls);

    string path(url);

    // The streaming services plugins set a trackId parameter in the
    // URIs. This gets parsed out by mhttpd. We rebuild a full url
    // which we pass to them for translation (they will extract the
    // trackid and use it, the rest of the path is bogus).
    // The uprcl module has a real path and no trackid. Handle both cases
    const char* stid =
        MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND,
                                    "trackId");
    if (stid && *stid) {
        path += string("?version=1&trackId=") + stid;
    }

    // Translate to Tidal/Qobuz etc real temporary URL
    string media_url = realplg->get_media_url(path);
    if (media_url.empty()) {
        LOGERR("answer_to_connection: no media_uri for: " << url << endl);
        return MHD_NO;
    }

    if (media_url.find("http") == 0) {
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
        LOGERR("PlgWithSlave: got non-http URL !: " << media_url << endl);
        LOGERR("PlgWithSlave:   the code for handling these is gone !\n");
        LOGERR("    will have to fetch it from git history\n");
        return MHD_NO;
    } 
}

static int accept_policy(void *, const struct sockaddr* sa, socklen_t addrlen)
{
    return MHD_YES;
}

// Called once for starting the Python program and do other initialization.
bool PlgWithSlave::Internal::maybeStartCmd()
{
    if (cmd.running()) {
        return true;
    }

    ConfSimple *conf = plg->m_services->getconfig(plg);
    int port = 49149;
    string sport;
    if (conf->get("plgmicrohttpport", sport)) {
        port = atoi(sport.c_str());
    }
    if (nullptr == mhd) {

        // Start the microhttpd daemon. There can be only one, and it
        // is started with a context handle which points to whatever
        // plugin got there first. The callback will only use the
        // handle to get to the plugin services, and retrieve the
        // appropriate plugin based on the url path prefix.
        LOGDEB("PlgWithSlave: starting httpd on port "<< port << endl);
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
            LOGERR("PlgWithSlave: MHD_start_daemon failed\n");
            return false;
        }
    }
    
    string pythonpath = string("PYTHONPATH=") +
        path_cat(g_datadir, "cdplugins") + ":" +
        path_cat(g_datadir, "cdplugins/pycommon") + ":" +
        path_cat(g_datadir, "cdplugins/" + plg->m_name);
    string configname = string("UPMPD_CONFIG=") + g_configfilename;
    stringstream ss;
    ss << upnphost << ":" << port;
    string hostport = string("UPMPD_HTTPHOSTPORT=") + ss.str();
    string pp = string("UPMPD_PATHPREFIX=") + pathprefix;
    if (!cmd.startCmd(exepath, {/*args*/},
                      /* env */ {pythonpath, configname, hostport, pp})) {
        LOGERR("PlgWithSlave::maybeStartCmd: startCmd failed\n");
        return false;
    }
    return true;
}

bool PlgWithSlave::startInit()
{
    return m && m->maybeStartCmd();
}

// Translate the slave-generated HTTP URL (based on the trackid), to
// an actual temporary service (e.g. tidal one), which will be an HTTP
// URL pointing to either an AAC or a FLAC stream.
// Older versions of this module handled AAC FLV transported over
// RTMP, apparently because of the use of a different API key. Look up
// the git history if you need this again.
// The Python code calls the service to translate the trackid to a temp
// URL. We cache the result for a few seconds to avoid multiple calls
// to tidal.
string PlgWithSlave::get_media_url(const string& path)
{
    LOGDEB0("PlgWithSlave::get_media_url: " << path << endl);
    if (!m->maybeStartCmd()) {
        return string();
    }
    time_t now = time(0);
    if (m->laststream.path.compare(path) ||
        (now - m->laststream.opentime > 10)) {
        unordered_map<string, string> res;
        if (!m->cmd.callproc("trackuri", {{"path", path}}, res)) {
            LOGERR("PlgWithSlave::get_media_url: slave failure\n");
            return string();
        }

        auto it = res.find("media_url");
        if (it == res.end()) {
            LOGERR("PlgWithSlave::get_media_url: no media url in result\n");
            return string();
        }
        m->laststream.clear();
        m->laststream.path = path;
        m->laststream.media_url = it->second;
        m->laststream.opentime = now;
    }

    LOGDEB("PlgWithSlave: media url [" << m->laststream.media_url << "]\n");
    return m->laststream.media_url;
}


PlgWithSlave::PlgWithSlave(const string& name, CDPluginServices *services)
    : CDPlugin(name, services)
{
    m = new Internal(this, services->getexecpath(this),
                     services->getupnpaddr(this),
                     services->getupnpport(this),
                     services->getpathprefix(this));
}

PlgWithSlave::~PlgWithSlave()
{
    delete m;
}

static void catstring(string& dest, const string& s2)
{
    if (s2.empty()) {
        return;
    }
    if (dest.empty()) {
        dest = s2;
    } else {
        dest += string(" ") + s2;
    }
}

static int resultToEntries(const string& encoded, int stidx, int cnt,
                           vector<UpSong>& entries)
{
    Json::Value decoded;
    istringstream input(encoded);
    input >> decoded;
    LOGDEB0("PlgWithSlave::results: got " << decoded.size() << " entries \n");
    LOGDEB1("PlgWithSlave::results: undecoded: " << decoded.dump() << endl);
    bool dolimit = cnt > 0;
    
    for (unsigned int i = stidx; i < decoded.size(); i++) {
#define JSONTOUPS(fld, nm) {catstring(song.fld, \
                                      decoded[i].get(#nm, "").asString());}
        if (dolimit && --cnt < 0) {
            break;
        }
        UpSong song;
        JSONTOUPS(id, id);
        JSONTOUPS(parentid, pid);
        JSONTOUPS(title, tt);
        JSONTOUPS(artUri, upnp:albumArtURI);
        JSONTOUPS(artist, upnp:artist);
        JSONTOUPS(upnpClass, upnp:class);
        JSONTOUPS(date, dc:date)
        // tp is container ("ct") or item ("it")
        string stp = decoded[i].get("tp", "").asString();
        if (!stp.compare("ct")) {
            song.iscontainer = true;
            string ss = decoded[i].get("searchable", "").asString();
            if (!ss.empty()) {
                song.searchable = stringToBool(ss);
            }
        } else  if (!stp.compare("it")) {
            song.iscontainer = false;
            JSONTOUPS(uri, uri);
            JSONTOUPS(artist, dc:creator);
            JSONTOUPS(genre, upnp:genre);
            JSONTOUPS(album, upnp:album);
            JSONTOUPS(tracknum, upnp:originalTrackNumber);
            JSONTOUPS(mime, res:mime);

            string ss = decoded[i].get("duration", "").asString();
            if (!ss.empty()) {
                song.duration_secs = atoi(ss.c_str());
            }
            ss = decoded[i].get("res:size", "").asString();
            if (!ss.empty()) {
                song.size = atoll(ss.c_str());
            }
            ss = decoded[i].get("res:bitrate", "").asString();
            if (!ss.empty()) {
                song.bitrate = atoi(ss.c_str());
            }
            ss = decoded[i].get("res:samplefreq", "").asString();
            if (!ss.empty()) {
                song.samplefreq = atoi(ss.c_str());
            }
            ss = decoded[i].get("res:channels", "").asString();
            if (!ss.empty()) {
                song.channels = atoi(ss.c_str());
            }
        } else {
            LOGERR("PlgWithSlave::result: bad type in entry: " << stp <<
                   "(title: " << song.title << ")\n");
            continue;
        }
        LOGDEB1("PlgWitSlave::result: pushing: " << song.dump() << endl);
        entries.push_back(song);
    }
    // We return the total match size, the count of actually returned
    // entries can be obtained from the vector
    return decoded.size();
}


class ContentCacheEntry {
public:
    ContentCacheEntry()
        : m_time(time(0)) {
    }
    int toResult(const string& classfilter, int stidx, int cnt,
                 vector<UpSong>& entries) const;
    time_t m_time;
    vector<UpSong> m_results;
};

int ContentCacheEntry::toResult(const string& classfilter, int stidx, int cnt,
                                vector<UpSong>& entries) const
{
    const vector<UpSong>& res = m_results;
    LOGDEB0("searchCacheEntryToResult: filter " << classfilter << " start " <<
            stidx << " cnt " << cnt << " res.size " << res.size() << endl);
    entries.reserve(cnt);
    int total = 0;
    for (unsigned int i = 0; i < res.size(); i++) {
        if (!classfilter.empty() && res[i].upnpClass.find(classfilter) != 0) {
            continue;
        }
        total++;
        if (stidx > int(i)) {
            continue;
        }
        if (cnt && int(entries.size()) >= cnt) {
            break;
        }
        LOGDEB1("ContentCacheEntry::toResult: pushing class " <<
                res[i].upnpClass << " tt " << res[i].title << endl);
        entries.push_back(res[i]);
    }
    return res.size();
}

class ContentCache {
public:
    ContentCache(int retention_secs = 300);
    ContentCacheEntry *get(const string& query);
    void set(const string& query, ContentCacheEntry &entry);
    void purge();
private:
    time_t m_lastpurge;
    int m_retention_secs;
    unordered_map<string, ContentCacheEntry> m_cache;
};

ContentCache::ContentCache(int retention_secs)
    : m_lastpurge(time(0)), m_retention_secs(retention_secs)
{
}

void ContentCache::purge()
{
    time_t now(time(0));
    if (now - m_lastpurge < 5) {
        return;
    }
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (now - it->second.m_time > m_retention_secs) {
            LOGDEB0("ContentCache::purge: erasing " << it->first << endl);
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
    m_lastpurge = now;
}

ContentCacheEntry *ContentCache::get(const string& key)
{
    purge();
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        LOGDEB0("ContentCache::get: found " << key << endl);
        // we return a copy of the vector. Make our multi-access life simpler...
        return new ContentCacheEntry(it->second);
    }
    LOGDEB0("ContentCache::get: not found " << key << endl);
    return nullptr;
}

void ContentCache::set(const string& key, ContentCacheEntry &entry)
{
    LOGDEB0("ContentCache::set: " << key << endl);
    m_cache[key] = entry;
}

// Cache for searches
static ContentCache o_scache(300);
// Cache for browsing
static ContentCache o_bcache(180);

// Better return a bogus informative entry than an outright error:
static int errorEntries(const string& pid, vector<UpSong>& entries)
{
    entries.push_back(
        UpSong::item(pid + "$bogus", pid,
                     "Service login or communication failure"));
    return 1;
}

// Note that the offset and count don't get to the plugin for
// now. Plugins just return a (plugin-dependant) fixed number of
// entries from offset 0, which we cache. There is no good reason for
// this, beyond the fact that we have to cap the entry count anyway,
// else the CP is going to read to the end which might be
// reaaaaalllllyyyy long.
int PlgWithSlave::browse(const string& objid, int stidx, int cnt,
                         vector<UpSong>& entries,
                         const vector<string>& sortcrits,
                         BrowseFlag flg)
{
    LOGDEB1("PlgWithSlave::browse\n");
    entries.clear();
    if (!m->maybeStartCmd()) {
        return errorEntries(objid, entries);
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

    string cachekey(m_name + ":" + objid);
    if (flg == CDPlugin::BFChildren) {
        // Check cache
        ContentCacheEntry *cep;
        if ((cep = o_bcache.get(cachekey)) != nullptr) {
            int total = cep->toResult("", stidx, cnt, entries);
            delete cep;
            return total;
        }
    }
    
    unordered_map<string, string> res;
    if (!m->cmd.callproc("browse", {{"objid", objid}, {"flag", sbflg}}, res)) {
        LOGERR("PlgWithSlave::browse: slave failure\n");
        return errorEntries(objid, entries);
    }

    auto ite = res.find("entries");
    if (ite == res.end()) {
        LOGERR("PlgWithSlave::browse: no entries returned\n");
        return errorEntries(objid, entries);
    }
    bool nocache = false;
    auto itc = res.find("nocache");
    if (itc != res.end()) {
        nocache = stringToBool(itc->second);
    }

    if (flg == CDPlugin::BFChildren) {
        ContentCacheEntry e;
        resultToEntries(ite->second, 0, 0, e.m_results);
        if (!nocache) {
            o_bcache.set(cachekey, e);
        }
        return e.toResult("", stidx, cnt, entries);
    } else {
        return resultToEntries(ite->second, stidx, cnt, entries);
    }
}

// Note that the offset and count don't get to the plugin for
// now. Plugins just return a (plugin-dependant) fixed number of
// entries from offset 0, which we cache. There is no good reason for
// this, beyond the fact that we have to cap the entry count anyway,
// else the CP is going to read to the end which might be
// reaaaaalllllyyyy long.
int PlgWithSlave::search(const string& ctid, int stidx, int cnt,
                         const string& searchstr,
                         vector<UpSong>& entries,
                         const vector<string>& sortcrits)
{
    LOGDEB("PlgWithSlave::search: [" << searchstr << "]\n");
    entries.clear();
    if (!m->maybeStartCmd()) {
        return errorEntries(ctid, entries);
    }

    // Computing a pre-cooked query. For simple-minded plugins.
    // Note that none of the qobuz/gmusic/tidal plugins actually use
    // the slavefield part (defining in what field the term should
    // match).
    // 
    // Ok, so the upnp query language is quite powerful, but us, not
    // so much. We get rid of parenthesis and then try to find the
    // first searchExp on a field we can handle, pretend the operator
    // is "contains" and just do it. I so don't want to implement a
    // parser for the query language when the services don't support
    // anything complicated anyway, and the users don't even want it...
    string ss;
    neutchars(searchstr, ss, "()");

    // The search had better be space-separated. no
    // upnp:artist="beatles" for you
    vector<string> vs;
    stringToStrings(ss, vs);

    // The sequence can now be either [field, op, value], or
    // [field, op, value, and/or, field, op, value,...]
    if ((vs.size() + 1) % 4 != 0) {
        LOGERR("PlgWithSlave::search: bad search string: [" << searchstr <<
               "]\n");
        return errorEntries(ctid, entries);
    }
    string slavefield;
    string value;
    string classfilter;
    string objkind;
    for (unsigned int i = 0; i < vs.size()-2; i += 4) {
        const string& upnpproperty = vs[i];
        LOGDEB("PlgWithSlave::search:clause: " << vs[i] << " " << vs[i+1] <<
               " " << vs[i+2] << endl);
        if (!upnpproperty.compare("upnp:class")) {
            // This defines -what- we are looking for (track/album/artist)
            const string& what(vs[i+2]);
            if (beginswith(what, "object.item")) {
                objkind = "track";
            } else if (beginswith(what, "object.container.person")) {
                objkind = "artist";
            } else if (beginswith(what, "object.container.musicAlbum") ||
                       beginswith(what, "object.container.album")) {
                objkind = "album";
            } else if (beginswith(what, "object.container.playlistContainer")
                       || beginswith(what, "object.container.playlist")) {
                objkind = "playlist";
            }
            classfilter = what;
        } else if (!upnpproperty.compare("upnp:artist") ||
                   !upnpproperty.compare("dc:author")) {
            slavefield = "artist";
            value = vs[i+2];
            break;
        } else if (!upnpproperty.compare("upnp:album")) {
            slavefield = "album";
            value = vs[i+2];
            break;
        } else if (!upnpproperty.compare("dc:title")) {
            slavefield = "track";
            value = vs[i+2];
            break;
        }
    }

    // In cache ?
    ContentCacheEntry *cep;
    string cachekey(m_name + ":" + ctid + ":" + searchstr);
    if ((cep = o_scache.get(cachekey)) != nullptr) {
        int total = cep->toResult(classfilter, stidx, cnt, entries);
        delete cep;
        return total;
    }

    // Run query
    unordered_map<string, string> res;
    if (!m->cmd.callproc("search", {
                {"objid", ctid},
                {"objkind", objkind},
                {"origsearch", searchstr},
                {"field", slavefield},
                {"value", value} },  res)) {
        LOGERR("PlgWithSlave::search: slave failure\n");
        return errorEntries(ctid, entries);
    }

    auto ite = res.find("entries");
    if (ite == res.end()) {
        LOGERR("PlgWithSlave::search: no entries returned\n");
        return errorEntries(ctid, entries);
    }
    bool nocache = false;
    auto itc = res.find("nocache");
    if (itc != res.end()) {
        nocache = stringToBool(itc->second);
    }
    // Convert the whole set and store in cache
    ContentCacheEntry e;
    resultToEntries(ite->second, 0, 0, e.m_results);
    if (!nocache) {
        o_scache.set(cachekey, e);
    }
    return e.toResult(classfilter, stidx, cnt, entries);
}
