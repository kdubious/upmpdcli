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

#include <string>
#include <vector>

#include "cmdtalk.h"

#include "pathut.h"
#include "log.hxx"
#include "json.hpp"

using namespace std;
extern string g_datadir, g_configfilename;

using json = nlohmann::json;

class Tidal::Internal {
public:
    Internal(const vector<string>& pth)
	: path(pth) {
    }
    CmdTalk cmd;
    bool maybeStartCmd() {
	LOGDEB("Tidal::maybeStartCmd\n");
	if (!cmd.running()) {
	    string pythonpath = string("PYTHONPATH=") +
		path_cat(g_datadir, "cdplugins/pycommon");
	    string configname = string("UPMPD_CONFIG=") +
		g_configfilename;
	    LOGDEB("Tidal::maybeStartCmd: calling startCmd\n");
	    if (!cmd.startCmd("tidal.py", {/*args*/},
			      {pythonpath, configname}, path)) {
		LOGDEB("Tidal::maybeStartCmd: startCmd failed\n");
		return false;
	    }
	    LOGDEB("Tidal::maybeStartCmd: startCmd ok\n");
	}
	LOGDEB("Tidal::maybeStartCmd: cmd running\n");
	return true;
    }

    vector<string> path;
};

Tidal::Tidal(const vector<string>& plgpath)
    : m(new Internal(plgpath))
{
}

Tidal::~Tidal()
{
    delete m;
}

int Tidal::browse(const std::string& objid, int stidx, int cnt,
		  std::vector<UpSong>& entries,
		  const std::vector<std::string>& sortcrits,
		  BrowseFlag flg)
{
    LOGDEB("Tidal::browse\n");
    if (!m->maybeStartCmd()) {
	LOGERR("Tidal::browse: startcmd failed\n");
	return -1;
    }
    unordered_map<string, string> res;
    if (!m->cmd.callproc("browse", {{"objid", objid}}, res)) {
	LOGERR("Tidal::browse: slave failure\n");
	return -1;
    }

    auto it = res.find("entries");
    if (it == res.end()) {
	LOGERR("Tidal::browse: no entries returned\n");
	return -1;
    }

    auto decoded = json::parse(it->second);
    LOGDEB("Tidal::browse: got " << decoded.size() << " entries\n");
    LOGDEB1("Tidal::browse: undecoded json: " << decoded.dump() << endl);
    
    for (const auto& it : decoded) {
	UpSong song;
	auto it1 = it.find("tp");
	if (it1 == it.end()) {
	    LOGERR("Tidal::browse: no type in entry\n");
	    continue;
	}
	string stp = it1.value();
	
#define JSONTOUPS(fld, nm)						\
	it1 = it.find(#nm);						\
	if (it1 != it.end()) {						\
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
    return decoded.size();
}
