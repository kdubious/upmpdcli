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
from upmplgmodels import Artist, Album, Track, Playlist, SearchResult, Category
from gmusicapi import Mobileclient

class Session(object):
    def __init__(self):
        self.api = None
        self.user = None

    def login(self, username, password):
        self.api = Mobileclient()

        logged_in = self.api.login(username, password,
                                   Mobileclient.FROM_MAC_ADDRESS)
        #print("Logged in: %s" % logged_in)
        #data = api.get_registered_devices()
        #print("registered: %s" % data)

        #isauth = api.is_authenticated()
        #print("Auth ok: %s" % isauth)
        return logged_in

    def get_media_url(self, song_id, quality=u'med'):
        url = self.api.get_stream_url(song_id, quality=quality)
        print("get_media_url got: %s" % url, file=sys.stderr)
        return url

    
    def search(self, query):
        data = self.api.search(query, max_results=5)
        print("Search results: %s" % json.dumps(data, indent=4), file=sys.stderr)
        #track = data['song_hits'][0]['track']
        #song_id = track['nid'] if 'nid' in track else track['id']

        tr = [_parse_track(i['track']) for i in data['song_hits']]
        ar = []
        al = []
        pl = []
        #ar = [_parse_artist(i) for i in data['artist_hits']]
        #al = [_parse_album(i) for i in data['albums_hits']]
        #pl = [_parse_playlist(i) for i in data['playlist_hits']]
        return SearchResult(artists=ar, albums=al, playlists=pl, tracks=tr)


def _parse_track(data, albumarg = None):

    artist_name = data['artist'] if 'artist' in data else "Unknown"
    artist = Artist(name = artist_name)

    #album_artist = data['albumArtist'] if 'albumArtist' in data else ""
    album_art = data['albumArtRef'][0]["url"] if 'albumArtRef' in data else ""
    album_tt = data['album'] if 'album' in data else "Unknown"
    album = Album(name=album_tt, image=album_art)

    kwargs = {
        'id': data['nid'] if 'nid' in data else data['id'],
        'name': data['title'],
        'duration': int(data['durationMillis'])/1000,
        'track_num': data['trackNumber'],
        'disc_num': data['discNumber'],
        'artist': artist,
        'album': album,
        'genre': data['genre']
        #'artists': artists,
    }

    return Track(**kwargs)


