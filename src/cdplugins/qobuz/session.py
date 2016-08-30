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



def _parse_artist(json_obj):
    artist = Artist(id=json_obj['id'], name=json_obj['name'])
    try:
        artist.role = Role(json_obj['type'])
    except:
        pass
    return artist


def _parse_track(json_obj):
    artist = _parse_artist(json_obj['performer'])
    #artists = _parse_artists(json_obj['artists'])
    #album = _parse_album(json_obj['album'], artist, artists)
    kwargs = {
        'id': json_obj['id'],
        'name': json_obj['title'],
        'duration': json_obj['duration'],
        'track_num': json_obj['track_number'],
        'disc_num': json_obj['media_number'],
        'artist': artist,
        #'artists': artists,
        #'album': album,
    }
    return Track(**kwargs)


class Favorites(object):

    def __init__(self, session):
        self.session = session

    def artists(self):
        return self.session._map_request(self._base_url + '/artists', ret='artists')

    def albums(self):
        return self.session._map_request(self._base_url + '/albums', ret='albums')

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
