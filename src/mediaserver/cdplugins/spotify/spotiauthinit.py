#!/usr/bin/python3

import spotipy
import sys
import json

import sys
import spotipy
import spotipy.util as util
from spotipy.oauth2 import SpotifyClientCredentials
import upmspotid

if len(sys.argv) > 1:
    username = sys.argv[1]
else:
    print("Whoops, need your username!")
    print("usage: spotipyauth [username]")
    sys.exit(1)

cachepath = "/tmp/spotipy-" + username + "-token"

token = util.prompt_for_user_token(
    username, scope = upmspotid.SCOPE, client_id = upmspotid.CLIENT_ID,
    client_secret = upmspotid.CLIENT_SECRET,
    redirect_uri = upmspotid.REDIRECT_URI, cache_path = cachepath)

if token:
    # Check
    sp = spotipy.Spotify(auth=token)
    data = sp.current_user_recently_played()
    if not data:
        print("Authentication failed");
        sys.exit(1)
    else:
        print("Authentication ok. Please move %s to the file named " \
              "/var/cache/upmpdcli/spotify/token" % cachepath)
        # print("%s"% json.dumps(data,indent=True))
        sys.exit(0)
else:
    print("Can't get token for %s"% username)
    sys.exit(1)
