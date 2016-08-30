# defined to emulate the session object from the tidal module, which is
# defined in the tidalapi part (we want to keep the qobuz api/ dir as
# much alone as possible.
from __future__ import print_function

import sys
from models import Artist, Album, Track, Playlist, SearchResult, Category, Role
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

    def get_media_url(self, trackid):
        # Format id: 5 for MP3 320, 6 for FLAC Lossless, 7 for FLAC
        # Hi-Res 24 bit =< 96kHz, 27 for FLAC Hi-Res 24 bit >96 kHz &
        # =< 192 kHz
        url = self.api.track_getFileUrl(intent="stream",
                                        track_id = trackid,
                                        format_id = 5)
        print("%s" % url, file=sys.stderr)
        return url['url'] if url and 'url' in url else None

    def get_album_tracks(self, albid):
        data = self.api.album_get(album_id = albid)
        album = _parse_album(data)
        #print("%s" % data, file=sys.stderr)
        return [_parse_track(t, album) for t in data['tracks']['items']]

    def get_artist_albums(self, artid):
        data = self.api.artist_get(artist_id=artid, extra='albums')
        #print("%s" % data, file=sys.stderr)
        if 'albums' in data:
            return [_parse_album(alb) for alb in data['albums']['items']]
        return []

    def get_artist_similar(self, artid):
        data = self.api.artist_getSimilarArtists(artist_id=artid)
        if 'artists' in data and 'items' in data['artists']:
            return [_parse_artist(art) for art in data['artists']['items']]
        return []

    def get_featured_items(self, content_type, type):
        if content_type == 'albums':
            # type : The type of recommandations to fetch:
            #   best-sellers, most-streamed, new-releases, press-awards,
            #   editor-picks, most-featured
            # genre_id - optional : The genre ID to filter the
            # recommandations by.
            data = self.api.album_getFeatured(type=type, limit='3')
            #print("%s" % data, file=sys.stderr)
            if 'albums' in data:
                return [_parse_album(alb) for alb in data['albums']['items']]
        return []

def _parse_artist(json_obj):
    artist = Artist(id=json_obj['id'], name=json_obj['name'])
    try:
        artist.role = Role(json_obj['type'])
    except:
        pass
    return artist

def _parse_album(json_obj, artist=None, artists=None):
    if artist is None and 'artist' in json_obj:
        artist = _parse_artist(json_obj['artist'])
    #if artists is None:
    #    artists = _parse_artists(json_obj['artists'])
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['title'],
        'num_tracks': json_obj.get('tracks_count'),
        'duration': json_obj.get('duration'),
        'artist': artist,
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

def _parse_track(json_obj, albumarg = None):
    if 'performer' in json_obj:
        artist = _parse_artist(json_obj['performer'])
    elif albumarg:
        artist = albumarg.artist
    else:
        artist = Artist()
    #artists = _parse_artists(json_obj['artists'])
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['title'],
        'duration': json_obj['duration'],
        'track_num': json_obj['track_number'],
        'disc_num': json_obj['media_number'],
        'artist': artist,
        #'artists': artists,
    }
    if 'album' in json_obj:
        kwargs['album'] = _parse_album(json_obj['album'], artist)
    elif albumarg:
        kwargs['album'] = albumarg
    return Track(**kwargs)


class Favorites(object):

    def __init__(self, session):
        self.session = session

    def artists(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'artists')
        #print("%s" % r, file=sys.stderr)
        return [_parse_artist(item) for item in r['artists']['items']]

    def albums(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'albums')
        #print("%s" % r, file=sys.stderr)
        return [_parse_album(item) for item in r['albums']['items']]

    def playlists(self):
        return self.session._map_request(self._base_url + '/playlists', ret='playlists')

    def tracks(self):
        r = self.session.api.favorite_getUserFavorites(
            user_id = self.session.user.id,
            type = 'tracks')
        #print("%s" % r, file=sys.stderr)
        return [_parse_track(item) for item in r['tracks']['items']]


class User(object):
    def __init__(self, session, id):
        self.session = session
        self.id = id
        self.favorites = Favorites(self.session)

    def playlists(self):
        return self.session.get_user_playlists(self.id)
