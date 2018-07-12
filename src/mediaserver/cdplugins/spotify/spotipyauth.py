#!/usr/bin/env python3

import spotipy
import sys
import json

# client id ? L_3DUkVpQRS0PW8uiGz8Yg
# shows a user's playlists (need to be authenticated via oauth)

import sys
import spotipy
import spotipy.util as util
from spotipy.oauth2 import SpotifyClientCredentials

def show_tracks(tracks):
    for i, item in enumerate(tracks['items']):
        track = item['track']
        print("   %d %32.32s %s" % (i, track['artists'][0]['name'],
            track['name']))


if __name__ == '__main__':
    if len(sys.argv) > 1:
        username = sys.argv[1]
    else:
        print("Whoops, need your username!")
        print("usage: python user_playlists.py [username]")
        sys.exit()

    token = util.prompt_for_user_token(username)

    if token:
        sp = spotipy.Spotify(auth=token)
        data = sp.user(username)
        print("%s"% json.dumps(data,indent=True))
        sys.exit(0)
        data = sp.album("2o9McLtDM7mbODV7yZF2mc")
        print("%s"% json.dumps(data,indent=True))
        data = sp.album_tracks("2o9McLtDM7mbODV7yZF2mc")
        print("%s"% json.dumps(data,indent=True))
        sys.exit(0)
        playlists = sp.user_playlists(username)
        for playlist in playlists['items']:
            if playlist['owner']['id'] == username:
                print("\n%s"%playlist['name'])
                print('  total tracks %d' % playlist['tracks']['total'])
                results = sp.user_playlist(username, playlist['id'],
                    fields="tracks,next")
                tracks = results['tracks']
                show_tracks(tracks)
                while tracks['next']:
                    tracks = sp.next(tracks)
                    show_tracks(tracks)
    else:
        print("Can't get token for %s"% username)
        

name = "moby"
offs = 0
lim = 20
while True:
    results = sp.search(q='artist:' + name, type='artist',limit=lim,offset=offs)
    
    print("%s"% json.dumps(results,indent=True))
    if len(results) != lim:
        break
    offs += len(results)

#lz_uri = 'spotify:artist:36QJpDe2go2KgaRleHCDTp'
#lz_uri = 'spotify:artist:2dJ1JCmbWQvxTCm1X7SqEq'
#lz_uri = 'spotify:artist:33lBD3LO109JfRCYElo5CZ'
lz_uri = 'spotify:artist:3OsRAKCvk37zwYcnzRf5XF'

results = sp.artist_top_tracks(lz_uri)

for track in results['tracks'][:10]:
    print('track    : %s'%track['name'])
    print('audio    : %s'%track['preview_url'])
    print('cover art: %s\n' % track['album']['images'][0]['url'])



sys.exit(0)



client_credentials_manager = SpotifyClientCredentials()
sp = spotipy.Spotify(client_credentials_manager=client_credentials_manager)

playlists = sp.user_playlists('spotify')
while playlists:
    for i, playlist in enumerate(playlists['items']):
        print("%4d %s %s" % (i + 1 + playlists['offset'], playlist['uri'],  playlist['name']))
    if playlists['next']:
        playlists = sp.next(playlists)
    else:
        playlists = None

sys.exit(0)


