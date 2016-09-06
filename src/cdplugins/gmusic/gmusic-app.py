#!/usr/bin/env python
#
# A lot of code copied from the Kodi Tidal addon which is:
# Copyright (C) 2014 Thomas Amland
#
# Additional code and modifications:
# Copyright (C) 2016 J.F.Dockes
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function
import sys
import os
import posixpath
import json
import re
import conftree
import cmdtalkplugin
from upmplgutils import *

# Using kodi plugin routing plugin: lets use reuse a lot of code from
# the addon. And much convenient in general
from routing import Plugin
# Need bogus base_url value to avoid plugin trying to call xbmc to
# retrieve addon id
plugin = Plugin('') 
from session import Session

# Func name to method mapper
dispatcher = cmdtalkplugin.Dispatch()
# Pipe message handler
msgproc = cmdtalkplugin.Processor(dispatcher)

session = Session()

is_logged_in = False

def maybelogin():
    global quality
    global httphp
    global pathprefix
    global is_logged_in
    
    if is_logged_in:
        return True

    if "UPMPD_HTTPHOSTPORT" not in os.environ:
        raise Exception("No UPMPD_HTTPHOSTPORT in environment")
    httphp = os.environ["UPMPD_HTTPHOSTPORT"]
    if "UPMPD_PATHPREFIX" not in os.environ:
        raise Exception("No UPMPD_PATHPREFIX in environment")
    pathprefix = os.environ["UPMPD_PATHPREFIX"]
    if "UPMPD_CONFIG" not in os.environ:
        raise Exception("No UPMPD_CONFIG in environment")
    upconfig = conftree.ConfSimple(os.environ["UPMPD_CONFIG"])
    
    username = upconfig.get('gmusicuser')
    password = upconfig.get('gmusicpass')
    quality = upconfig.get('gmusicquality')
    if quality not in ('hi', 'med', 'low'):
        raise Exception("gmusic bad quality value: " + quality)
        
    if not username or not password:
        raise Exception("gmusicuser and/or gmusicpass not set in configuration")

    is_logged_in = session.login(username, password)
    
    if not is_logged_in:
        raise Exception("gmusic login failed")


def get_mimeandkbs():
    if quality == 'hi':
        return ('audio/mpeg', str(320))
    elif quality == 'med':
        return ('audio/mpeg', str(160))
    elif quality == 'low':
        return ('audio/mpeg', str(128))
    else:
        return ('audio/mpeg', str(128))


@dispatcher.record('trackuri')
def trackuri(a):
    global quality
    msgproc.log("trackuri: [%s]" % a)
    trackid = trackid_from_urlpath(pathprefix, a)
    maybelogin()
    media_url = session.get_media_url(trackid, quality)
    mime, kbs = get_mimeandkbs()
    return {'media_url' : media_url, 'mimetype' : mime, 'kbs' : kbs}


# Bogus global for helping with reusing kodi addon code
class XbmcPlugin:
    SORT_METHOD_TRACKNUM = 1
    def __init__(self):
        self.entries = []
        self.objid = ''
    def addDirectoryItem(self, hdl, endpoint, title, isend):
        self.entries.append(direntry('0$gmusic$' + endpoint, self.objid, title))
        return
    def endOfDirectory(self, h):
        return
    def setContent(self, a, b):
        return
    def addSortMethod(self, a, b):
        return
    
def add_directory(title, endpoint):
    if callable(endpoint):
        endpoint = plugin.url_for(endpoint)
    xbmcplugin.entries.append(direntry('0$gmusic$' + endpoint, xbmcplugin.objid, title))

def urls_from_id(view_func, items):
    #msgproc.log("urls_from_id: items: %s" % str([item.id for item in items]))
    return [plugin.url_for(view_func, item.id) for item in items if str(item.id).find('http') != 0]

def view(data_items, urls, end=True):
    for item, url in zip(data_items, urls):
        title = item.name
        xbmcplugin.entries.append(direntry('0$gmusic$' + url, xbmcplugin.objid, title))

def track_list(tracks):
    xbmcplugin.entries += trackentries(httphp, pathprefix,
                                       xbmcplugin.objid, tracks)


@dispatcher.record('browse')
def browse(a):
    global xbmcplugin
    xbmcplugin = XbmcPlugin()
    msgproc.log("browse: [%s]" % a)
    if 'objid' not in a:
        raise Exception("No objid in args")
    objid = a['objid']
    bflg = a['flag'] if 'flag' in a else 'children'
    
    if re.match('0\$gmusic\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)
    maybelogin()

    xbmcplugin.objid = objid
    idpath = objid.replace('0$gmusic$', '', 1)
    if bflg == 'meta':
        m = re.match('.*\$(.+)$', idpath)
        if m:
            trackid = m.group(1)
            track = session.get_track(trackid)
            track_list([track])
    else:
        plugin.run([idpath])
    #msgproc.log("%s" % xbmcplugin.entries)
    encoded = json.dumps(xbmcplugin.entries)
    return {"entries" : encoded}

@plugin.route('/')
def root():
    add_directory('Library', my_music)

@plugin.route('/my_music')
def my_music():
    add_directory('Albums', my_music)

#data = api.get_all_songs()
#print("songs: %s" % json.dumps(data,indent=4))

@dispatcher.record('search')
def search(a):
    global xbmcplugin
    xbmcplugin = XbmcPlugin()
    msgproc.log("search: [%s]" % a)
    objid = a['objid']
    field = a['field']
    value = a['value']
    if re.match('0\$gmusic\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)
    xbmcplugin.objid = objid
    maybelogin()
    
    searchresults = session.search(value)

    if field not in ['artist', 'album', 'playlist', 'track']:
        msgproc.log('Unknown field \'%s\'' % field)
        field = None
    if field is None or field == 'artist':
        view(searchresults.artists,
             urls_from_id(artist_view, searchresults.artists), end=False)
    if field is None or field == 'album':
        view(searchresults.albums,
             urls_from_id(album_view, searchresults.albums), end=False)
    if field is None or field == 'playlist':
        view(searchresults.playlists,
             urls_from_id(playlist_view, searchresults.playlists), end=False)
        
    if field is None or field == 'track':
        track_list(searchresults.tracks)

    #msgproc.log("%s" % xbmcplugin.entries)
    encoded = json.dumps(xbmcplugin.entries)
    return {"entries" : encoded}

msgproc.log("Qobuz running")
msgproc.mainloop()
