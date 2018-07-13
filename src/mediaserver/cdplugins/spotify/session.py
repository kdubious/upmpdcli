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

import spotipy
import spotipy.util as spotutil

from upmplgmodels import Artist, Album, Track, Playlist, SearchResult, \
     Category, Genre
from upmplgutils import uplog

import upmspotid

class Session(object):
    def __init__(self):
        self.api = None
        self.user = None
        
    def dmpdata(self, who, data):
        uplog("%s: %s" % (who, json.dumps(data, indent=4)))

    def login(self, user, cachepath):
        self.user = user
        scope = upmspotid.SCOPE
        sp_oauth = spotipy.oauth2.SpotifyOAuth(
            upmspotid.CLIENT_ID, upmspotid.CLIENT_SECRET,
            upmspotid.REDIRECT_URI, scope=upmspotid.SCOPE, cache_path=cachepath)
        token_info = sp_oauth.get_cached_token()
        if token_info:
            uplog("token_info: %s" % token_info)
            uplog("token expires at %s" % datetime.datetime.fromtimestamp(
                token_info['expires_at']).strftime('%Y-%m-%d %H:%M:%S'))
            self.api = spotipy.Spotify(auth=token_info['access_token'])
            data = self.api.user(self.user)
            #dmpdata("User basic info", data)
            return True
        return False
    
    def recent_tracks(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.current_user_recently_played()
        #self.dmpdata('user_recently_played', data)
        return [_parse_track(i['track']) for i in data['items']]

    def favourite_tracks(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.current_user_top_tracks(limit=50, offset=0)
        return [_parse_track(t) for t in data['items']]
        
    def favourite_albums(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.current_user_saved_albums()
        #self.dmpdata('favourite_albums', data)
        try:
            return [_parse_album(item['album']) for item in data['items']]
        except:
            uplog("favourite_albums: _parse_albums failed")
            pass
        return []

    def favourite_artists(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.current_user_followed_artists()
        #self.dmpdata('favourite_artists', data)
        return [_parse_artist(item) for item in data['artists']['items']]

    def get_artist_albums(self, id):
        data = self.api.artist_albums(id, limit=50)
        #self.dmpdata('get_artist_albums', data)
        return [_parse_album(item) for item in data['items']]
        
    def new_releases(self):
        if not self.api:
            uplog("Not logged in")
            return []
        data = self.api.new_releases()
        #self.dmpdata('new_releases', data)
        try:
            return [_parse_album(alb) for alb in data['albums']['items']]
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

def _parse_track(data, albumarg = None):
    artist = Artist()
    if 'artists' in data:
        artist = _parse_artist(data['artists'][0])
    elif albumarg and albumarg.artist:
        artist = albumarg.artist

    available = True
    duration = 0
    if 'duration_ms' in data:
        duration = str(int(data['duration_ms'])//1000)
    kwargs = {
        'id': data['id'],
        'name': data['name'],
        'duration': duration,
        'track_num': data['track_number'],
        'disc_num': data['disc_number'],
        'artist': artist,
        'available': available
    }
    if albumarg:
        kwargs['album'] = albumarg
    elif 'album' in data:
        kwargs['album'] = _parse_album(data['album'])
    return Track(**kwargs)


def _parse_artist(json_obj):
    artist = Artist(id=json_obj['id'], name=json_obj['name'])
    return artist
