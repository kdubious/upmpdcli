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

# Using kodi plugin routing plugin: lets use reuse a lot of code from
# the addon.
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
    
    username = upconfig.get('qobuzuser')
    password = upconfig.get('qobuzpass')
    #qalstr = upconfig.get('qobuzquality')
    if not username or not password:
        raise Exception("qobuzuser and/or qobuzpass not set in configuration")

    is_logged_in = session.login(username, password)
    
def trackentries(objid, tracks):
    entries = []
    for track in tracks:
        li = {}
        li['pid'] = objid
        li['id'] = objid + '$' + "%s" % track.id
        li['tt'] = track.name
        li['uri'] = 'http://%s' % httphp + \
                    posixpath.join(pathprefix,
                                   'track?version=1&trackId=%s' % \
                                   track.id)
        #msgproc.log("URI: [%s]" % li['uri'])
        li['tp'] = 'it'
        if track.album:
            li['upnp:album'] = track.album.name
            if track.album.image:
                li['upnp:albumArtURI'] = track.album.image
            if track.album.release_date:
                li['releasedate'] = track.album.release_date 
        li['upnp:originalTrackNumber'] =  str(track.track_num)
        li['upnp:artist'] = track.artist.name
        li['dc:title'] = track.name
        li['discnumber'] = str(track.disc_num)
        li['duration'] = track.duration
        entries.append(li)
    return entries

def direntry(id, pid, title):
    return {'id': id, 'pid' : pid, 'tt': title, 'tp':'ct'}

def direntries(objid, ttidlist):
    content = []
    for tt,id in ttidlist:
        content.append(direntry(objid + '$' + id, objid, tt))
    return content

def trackid_from_urlpath(a):
    if 'path' not in a:
        raise Exception("No path in args")
    path = a['path']

    # pathprefix + 'track?version=1&trackId=trackid
    exp = posixpath.join(pathprefix, '''track\?version=1&trackId=(.+)$''')
    m = re.match(exp, path)
    if m is None:
        raise Exception("trackuri: path [%s] does not match [%s]" % (path, exp))
    trackid = m.group(1)
    return trackid

@dispatcher.record('trackuri')
def trackuri(a):
    msgproc.log("trackuri: [%s]" % a)
    trackid = trackid_from_urlpath(a)
    maybelogin()
    media_url = session.get_media_url(trackid)
    #msgproc.log("%s" % media_url)
    mime = "audio/mpeg"
    kbs = "128"
    return {'media_url' : media_url, 'mimetype' : mime, 'kbs' : kbs}


# Bogus global for helping with reusing kodi addon code
class XbmcPlugin:
    SORT_METHOD_TRACKNUM = 1
    def __init__(self):
        self.entries = []
        self.objid = ''
    def addDirectoryItem(self, hdl, endpoint, title, isend):
        self.entries.append(direntry('0$qobuz$' + endpoint, self.objid, title))
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
    xbmcplugin.entries.append(direntry('0$qobuz$' + endpoint, xbmcplugin.objid, title))

def track_list(tracks):
    xbmcplugin.entries += trackentries(xbmcplugin.objid, tracks)

@dispatcher.record('browse')
def browse(a):
    global xbmcplugin
    xbmcplugin = XbmcPlugin()
    msgproc.log("browse: [%s]" % a)
    if 'objid' not in a:
        raise Exception("No objid in args")
    objid = a['objid']
    bflg = a['flag'] if 'flag' in a else 'children'
    
    if re.match('0\$qobuz\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)
    maybelogin()

    xbmcplugin.objid = objid
    idpath = objid.replace('0$qobuz$', '', 1)
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
    add_directory("Discover", whats_new)
    add_directory('Favourites', my_music)
    add_directory('Playlists', playlists)

@plugin.route('/whats_new')
def whats_new():
    pass

@plugin.route('/my_music')
def my_music():
    #add_directory('Albums', favourite_albums)
    add_directory('Tracks', favourite_tracks)
    #add_directory('Artists', favourite_artists)
    xbmcplugin.endOfDirectory(plugin.handle)
    pass

@plugin.route('/favourite_tracks')
def favourite_tracks():
    track_list(session.user.favorites.tracks())

@plugin.route('/playlists')
def playlists():
    pass

def nono():
    maybelogin()
    if is_logged_in:
        msgproc.log("logged in")
        data = session.album_getFeatured(type='most-streamed',
                                         offset=0,
                                         limit=100)
        if len(data['albums']['items']) == 0:
            raise Exception("Empty album list")
        #msgproc.log("%s"%data)
        for albumdata in data['albums']['items']:
            albumid = albumdata['id']
            onealbumdata = session.album_get(album_id = albumid)
            #msgproc.log("%s"%json.dumps(data, indent=4))
            for track in onealbumdata['tracks']['items']:
                #msgproc.log("%s"%json.dumps(track, indent=4))
                if track['streamable']:
                    trackid = track['id']
                    url = session.track_getFileUrl(intent="stream",
                                                   track_id = trackid,
                                                   format_id = 4)
                    #msgproc.log("%s" % url['url'])
                    sys.exit(0)
                else:
                    print("Track not streamable")


msgproc.log("Qobuz running")
msgproc.mainloop()
