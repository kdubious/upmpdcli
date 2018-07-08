# Copyright (C) 2016 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
from __future__ import print_function

import sys
import json
import datetime
import time
import os
from upmplgmodels import Artist, Album, Track, Playlist, SearchResult, \
     Category, Genre
from upmplgutils import uplog

import spotipy
import spotipy.util as spotutil

SPOTIPY_CLIENT_ID = '5b4e1f241a734668aaddf170017d9244'
SPOTIPY_CLIENT_SECRET = '9bd49518f7974f8ebb6f965ae641022b'
SPOTIPY_REDIRECT_URI = 'https://www.lesbonscomptes.com/spotify/'

class Session(object):
    def __init__(self):
        self.api = None
        self.user = None
        
    def dmpdata(self, who, data):
        uplog("%s: %s" % (who, json.dumps(data, indent=4)))

    def login(self, user, cachepath):
        self.user = user
        scope = None
        sp_oauth = spotipy.oauth2.SpotifyOAuth(
            SPOTIPY_CLIENT_ID, SPOTIPY_CLIENT_SECRET, SPOTIPY_REDIRECT_URI, 
            scope=scope, cache_path=cachepath)
        token_info = sp_oauth.get_cached_token()
        if token_info:
            uplog("token_info: %s" % token_info)
            self.api = spotipy.Spotify(auth=token_info['access_token'])
            data = self.api.user(self.user)
            #dmpdata("User basic info", data)
            return True
        return False
    
    def top_tracks(self):
        if not self.api:
            uplog("Not logged in")
            return []
        return []

    def new_releases(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.new_releases()
        #self.dmpdata('new_releases', data)
        try:
            albums = [_parse_album(alb) for alb in data['albums']['items']]
            if albums:
                return [alb for alb in albums if alb.available]
        except:
            uplog("new_releases: _parse_albums failed")
            pass
        return []

    def get_album_tracks(self, albid):
        data = self.api.album(album_id = albid)
        album = _parse_album(data)
        return [_parse_track(t, album) for t in data['tracks']['items']]

def _parse_album(json_obj, artist=None, artists=None):
    #uplog("_parse_album: %s" % json_obj)
    if artist is None and 'artists' in json_obj:
        artist = _parse_artist(json_obj['artists'][0])
    available = True
    #if not available:
    #    uplog("Album not streamable: %s " % json_obj['title'])
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['name'],
        #'num_tracks': json_obj.get('tracks_count'),
        #'duration': json_obj.get('duration'),
        'artist': artist,
        'available': available,
        #'artists': artists,
    }

    if 'images' in json_obj:
        kwargs['image'] = json_obj['images'][0]['url']
        
    if 'releaseDate' in json_obj:
        try:
            # Keep this as a string else we fail to json-reserialize it later
            kwargs['release_date'] = json_obj['releaseDate']
        except ValueError:
            pass
    return Album(**kwargs)

def _parse_track(json_obj, albumarg = None):
    artist = Artist()
    if 'artists' in json_obj:
        artist = _parse_artist(json_obj['artists'][0])
    elif albumarg and albumarg.artist:
        artist = albumarg.artist

    available = True
    duration = 0
    if 'duration_ms' in json_obj:
        duration = str(int(json_obj['duration_ms'])//1000)
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['name'],
        'duration': duration,
        'track_num': json_obj['track_number'],
        'disc_num': json_obj['disc_number'],
        'artist': artist,
        'available': available
    }
    if albumarg:
        kwargs['album'] = albumarg

    return Track(**kwargs)


def _parse_artist(json_obj):
    artist = Artist(id=json_obj['id'], name=json_obj['name'])
    return artist
