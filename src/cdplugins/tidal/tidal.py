#!/usr/bin/env python
# Copyright (C) 2016 J.F.Dockes
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

from __future__ import print_function

import sys
import os
import json
import re
import conftree
import cmdtalkplugin
import tidalapi
from tidalapi.models import Album, Artist
from tidalapi import Quality

routes = cmdtalkplugin.Routes()
processor = cmdtalkplugin.Processor(routes)

is_logged_in = False

def maybelogin():
    global session
    global quality
    
    if is_logged_in:
        return True

    if "UPMPD_CONFIG" not in os.environ:
        raise Exception("No UPMPD_CONFIG in environment")
    upconfig = conftree.ConfSimple(os.environ["UPMPD_CONFIG"])
    
    username = upconfig.get('tidaluser')
    password = upconfig.get('tidalpass')
    qalstr = upconfig.get('tidalquality')
    if not username or not password:
        raise Exception("tidaluser and/or tidalpass not set in configuration")

    if qalstr == 'lossless':
        quality = Quality.lossless
    elif qalstr == 'high':
        quality = Quality.high
    elif qalstr == 'low':
        quality = Quality.low
    elif not qalstr:
        quality = Quality.high
    else:
        raise Exception("Bad tidal quality string in config. "+
                        "Must be lossless/high/low")
    tidalconf = tidalapi.Config(quality)
    session = tidalapi.Session(config=tidalconf)
    session.login(username, password)



def trackentries(objid, tracks):
    entries = []
    for track in tracks:
        if not track.available:
            continue
        li = {}
        

        li['pid'] = objid
        li['id'] = objid + '$' + "%s"%track.id
        li['tt'] = track.name
        li['uri'] = 'http://s.com/upmpcli-tidal/track?version=1&trackId=%s' % track.id
        li['tp'] = 'it'
        if track.album:
            li['upnp:album'] = track.album.name
            if track.album.image:
                li['upnp:albumArtUri'] = track.album.image
        li['upnp:originalTrackNumber'] =  str(track.track_num)
        li['upnp:artist'] = track.artist.name
        li['dc:creator'] = track.artist.name
        li['dc:title'] = track.name
        li['discnumber'] = str(track.disc_num)
        li['duration'] = track.duration
        # track.album.release_date.year if track.album.release_date else None,

        entries.append(li)
        return entries


def direntry(id, pid, title):
    return {'id': id, 'pid' : pid, 'tt':title, 'tp':'ct'}

def direntries(objid, ttidlist):
    content = []
    for tt,id in ttidlist:
        content.append(direntry(objid+'$'+id, objid, tt))
    return content

@routes.route('browse')
def browse(a):
    processor.rclog("browse: [%s]" % a)
    if 'objid' not in a:
        raise Exception("No objid in args")
    objid = a['objid']
    if re.match('0\$tidal', objid) is None:
        raise Exception("bad objid [%s]" % objid)

    maybelogin()
    if objid == '0$tidal':
        # Root
        content = direntries(objid,
                              (('My Music', 'my_music'),
                               ('Featured Playlists', 'featured'),
                               ("What's New", 'new'),
                               ('Genres', 'genres'),
                               ('Moods', 'moods'),
                               ('Search', 'search')))
    elif objid == '0$tidal$my_music':
        content = direntries(objid,
                             (('My Playlists', 'my_pl'),
                              ('Favourite Playlists', 'fav_pl'),
                              ('Favourite Artists', 'fav_art'),
                              ('Favourite Albums', 'fav_alb'),
                              ('Favourite Tracks', 'fav_trk')))
    elif objid == '0$tidal$my_music$fav_trk':
        content = trackentries(objid, session.user.favorites.tracks())
    elif objid == '0$tidal$my_music$my_pl':
        items = session.user.playlists()
        view(items, urls_from_id(playlist_view, items))
    elif objid == '0$tidal$my_music$fav_pl':
        items = session.user.favorites.playlists()
        view(items, urls_from_id(playlist_view, items))
    elif objid == '0$tidal$my_music$fav_art':
        xbmcplugin.setContent(plugin.handle, 'artists')
        items = session.user.favorites.artists()
        view(items, urls_from_id(artist_view, items))
    elif objid == '0$tidal$my_music$fav_alb':
        xbmcplugin.setContent(plugin.handle, 'albums')
        items = session.user.favorites.albums()
        view(items, urls_from_id(album_view, items))
    else:
        raise Exception("unhandled path")

    encoded = json.dumps(content)
    return {"entries":encoded}



processor.rclog("Tidal running")
processor.mainloop()
