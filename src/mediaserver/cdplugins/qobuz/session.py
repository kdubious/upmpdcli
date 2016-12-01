# defined to emulate the session object from the tidal module, which is
# defined in the tidalapi part (we want to keep the qobuz api/ dir as
# much alone as possible.
from __future__ import print_function

import sys
import json
from upmplgmodels import Artist, Album, Track, Playlist, SearchResult, \
     Category, Genre
from upmplgutils import *
from qobuz.api import raw

class Session(object):
    def __init__(self):
        self.api = None
        self.user = None

    def login(self, username, password):
        self.api = raw.RawApi()
        data = self.api.user_login(username = username, password = password)
        if data:
            self.user = User(self, self.api.user_id)

            return True
        else:
            return False

    def get_media_url(self, trackid, format_id=5):
        # Format id: 5 for MP3 320, 6 for FLAC Lossless, 7 for FLAC
        # Hi-Res 24 bit =< 96kHz, 27 for FLAC Hi-Res 24 bit >96 kHz &
        # =< 192 kHz
        url = self.api.track_getFileUrl(intent="stream",
                                        track_id = trackid,
                                        format_id = format_id)
        uplog("get_media_url got: %s" % url)
        return url['url'] if url and 'url' in url else None

    def get_album_tracks(self, albid):
        data = self.api.album_get(album_id = albid)
        album = _parse_album(data)
        return [_parse_track(t, album) for t in data['tracks']['items']]

    def get_playlist_tracks(self, plid):
        data = self.api.playlist_get(playlist_id = plid, extra = 'tracks')
        #uplog("PLAYLIST: %s" % json.dumps(data, indent=4))
        return [_parse_track(t) for t in data['tracks']['items']]

    def get_artist_albums(self, artid):
        data = self.api.artist_get(artist_id=artid, extra='albums')
        if 'albums' in data:
            albums = [_parse_album(alb) for alb in data['albums']['items']]
            return [alb for alb in albums if alb.available]
        return []

    def get_artist_similar(self, artid):
        data = self.api.artist_getSimilarArtists(artist_id=artid)
        if 'artists' in data and 'items' in data['artists']:
            return [_parse_artist(art) for art in data['artists']['items']]
        return []

    def get_featured_albums(self, genre_id=None, type='new-releases'):
        #uplog("get_featured_albums, genre_id %s type %s " % (genre_id, type))
        data = self.api.album_getFeatured(type=type,
                                          genre_id=genre_id, limit=100)
        try:
            albums = [_parse_album(alb) for alb in data['albums']['items']]
            if albums:
                return [alb for alb in albums if alb.available]
        except:
            pass
        return []

    def get_featured_playlists(self, genre_id=None):
        data = self.api.playlist_getFeatured(type='editor-picks',
                                             genre_id=genre_id, limit=100)
        if data and 'playlists' in data:
            return [_parse_playlist(pl) for pl in data['playlists']['items']]
        return []

    # content_type: albums/artists/playlists.  type : The type of
    # recommandations to fetch: best-sellers, most-streamed,
    # new-releases, press-awards, editor-picks, most-featured
    # In practise, and despite the existence of the
    # catalog_getFeaturedTypes call which returns the above list, I
    # could not find a way to pass the type parameter to
    # catalog_getFeatured (setting type triggers an
    # error). album_getFeatured() accepts type, but it's not clear
    # what it does.
    def get_featured_items(self, content_type, type=''):
        #uplog("FEATURED TYPES: %s" % self.api.catalog_getFeaturedTypes())
        limit = '100'
        data = self.api.catalog_getFeatured(limit=limit)
        #uplog("Featured: %s" % json.dumps(data,indent=4)))
        if content_type == 'artists':
            if 'artists' in data:
                return [_parse_artist(i) for i in data['artists']['items']]
        elif content_type == 'playlists':
            if 'playlists' in data:
                return [_parse_playlist(pl) for pl in data['playlists']['items']]
        elif content_type == 'albums':
            if 'albums' in data:
                return [_parse_album(alb) for alb in data['albums']['items']]
        return []

    def get_genres(self, parent=None):
        data = self.api.genre_list(parent_id=parent)
        return [_parse_genre(g) for g in data['genres']['items']]

    def _search1(self, query, tp):
        limit = 200
        slice = 100
        if tp == 'artists':
            limit = 20
            slice = 20
        elif tp == 'albums' or tp == 'playlists':
            # I think that qobuz actually imposes a limit of
            # 20 for album searches.
            limit = 50
            slice = 20
        offset = 0
        ar = []
        al = []
        pl = []
        tr = []
        while offset < limit:
            data = self.api.catalog_search(query=query, type=tp,
                                           offset=offset, limit=slice)
            try:
                ar_ = [_parse_artist(i) for i in data['artists']['items']]
            except:
                ar_ = []
            try:
                al_ = [_parse_album(i) for i in data['albums']['items']]
                al_ = [alb for alb in al_ if alb.available]
            except:
                al_ = []
            try:
                pl_ = [_parse_playlist(i) for i in data['playlists']['items']]
            except:
                pl_ = []
            try:
                tr_ = [_parse_track(i) for i in data['tracks']['items']]
            except:
                tr_ = []
            ar.extend(ar_)
            al.extend(al_)
            pl.extend(pl_)
            tr.extend(tr_)
            offset += slice
        
        uplog("_search1: got %d artists %d albs %d tracks %d pl" %
              (len(ar), len(al), len(tr), len(pl)))
        return SearchResult(artists=ar, albums=al, playlists=pl, tracks=tr)

    def search(self, query, tp):
        if tp:
            return self._search1(query, tp)
        else:
            cplt = SearchResult()
            res = self._search1(query, 'artists')
            cplt.artists = res.artists
            res = self._search1(query, 'albums')
            cplt.albums = res.albums
            res = self._search1(query, 'tracks')
            cplt.tracks = res.tracks
            res = self._search1(query, 'playlists')
            cplt.playlists = res.playlists
            return cplt

def _parse_artist(json_obj):
    artist = Artist(id=json_obj['id'], name=json_obj['name'])
    return artist

def _parse_genre(data):
    return Genre(id=data['id'], name=data['name'])

def _parse_album(json_obj, artist=None, artists=None):
    if artist is None and 'artist' in json_obj:
        artist = _parse_artist(json_obj['artist'])
    #if artists is None:
    #    artists = _parse_artists(json_obj['artists'])
    available = json_obj['streamable'] if 'streamable' in json_obj else false
    #if not available:
    #    uplog("Album not streamable: %s " % json_obj['title'])
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['title'],
        'num_tracks': json_obj.get('tracks_count'),
        'duration': json_obj.get('duration'),
        'artist': artist,
        'available': available,
        #'artists': artists,
    }
    if 'image' in json_obj and 'large' in json_obj['image']:
        kwargs['image'] = json_obj['image']['large']
        
    if 'releaseDate' in json_obj:
        try:
            kwargs['release_date'] = datetime.datetime(*map(int, json_obj['releaseDate'].split('-')))
        except ValueError:
            pass
    return Album(**kwargs)


def _parse_playlist(json_obj, artist=None, artists=None):
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['name'],
        'num_tracks': json_obj.get('tracks_count'),
        'duration': json_obj.get('duration'),
    }
    return Playlist(**kwargs)

def _parse_track(json_obj, albumarg = None):
    artist = Artist()
    if 'performer' in json_obj:
        artist = _parse_artist(json_obj['performer'])
    elif 'artist' in json_obj:
        artist = _parse_artist(json_obj['artist'])
    elif albumarg and albumarg.artist:
        artist = albumarg.artist

    album = None
    if 'album' in json_obj:
        album = _parse_album(json_obj['album'], artist)
    else:
        album = albumarg

    available = json_obj['streamable'] if 'streamable' in json_obj else false
    #if not available:
    #uplog("Track no not streamable: %s " % json_obj['title'])

    #artists = _parse_artists(json_obj['artists'])
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['title'],
        'duration': json_obj['duration'],
        'track_num': json_obj['track_number'],
        'disc_num': json_obj['media_number'],
        'artist': artist,
        'available': available
        #'artists': artists,
    }
    if album:
        kwargs['album'] = album

    return Track(**kwargs)


class Favorites(object):

    def __init__(self, session):
        self.session = session

    def artists(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'artists')
        #uplog("%s" % r)
        return [_parse_artist(item) for item in r['artists']['items']]

    def albums(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'albums')
        #uplog("%s" % r)
        albums = [_parse_album(item) for item in r['albums']['items']]
        return [alb for alb in albums if alb.available]

    def playlists(self):
        r = self.session.api.playlist_getUserPlaylists()
        return [_parse_playlist(item) for item in r['playlists']['items']]

    def tracks(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'tracks')
        #uplog("%s" % r)
        return [_parse_track(item) for item in r['tracks']['items']]


class User(object):
    def __init__(self, session, id):
        self.session = session
        self.id = id
        self.favorites = Favorites(self.session)

    def playlists(self):
        return self.session.get_user_playlists(self.id)
