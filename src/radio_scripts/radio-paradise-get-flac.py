#!/usr/bin/env python3
# Copyright (C) 2017-2018 J.F.Dockes
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

# Handling the Flac channel from radio-paradise. This is made of
# 'blocks' which are multi-title periods with a single audio
# url. Metadata should be displayed according to the elapsed time in
# the audio (each metadata entry in the array gives the start and
# duration for the song in mS).  If metadata is re-fetched during the
# block, it is front-truncated, more or less according to the current
# time, and it may happen that the metadata for the song playing
# locally is not there any more (someone starting at this point would
# begin with the next point). The thing which does not change for
# successive fetches of the same block is the "end_event" value. We
# cache the first fetch for any block so that we are sure to have the
# right metadata available when upmpdcli calls us


import requests
import json
import sys
import os

tmpname = '/tmp/up-rp-pldata.json'

def debug(x):
    print("radio-paradise-get-flac.py: %s" % x, file=sys.stderr)
    pass

# Write new block data to cache. We always output the audio url in this case.
def newcache(jsd):
    global out
    s = json.dumps(jsd,sort_keys=True, indent=4)
    open(tmpname, 'wb').write(s.encode('utf-8'))
    os.chmod(tmpname, 0o666)
    out["audioUrl"] = jsd['url'] + "?src=alexa"

##### Main script

    
# We're expecting args as successive pairs of "name value", and we
# need elapsedms (elapsed milliseconds in current track)
# (e.g. myscript elapsedms 134)
if (len(sys.argv)+1) % 2:
    debug("argv len not odd")
    sys.exit(1)
args = {}
for i in range(len(sys.argv)-2):
    nm = sys.argv[i+1]
    val = sys.argv[i+2]
    args[nm] = val

elapsedms = -1
try:
    elapsedms = int(args["elapsedms"])
except:
    pass

debug("rp-get-flac: got elapsed %d" % elapsedms)

# Try to read the current cached data.
cached = None
try:
    s = open(tmpname, 'rb').read().decode('utf-8', errors = 'replace')
    cached = json.loads(s)
except Exception as err:
    debug("No cached data read: %s" % err)
    pass


r = requests.get("https://api.radioparadise.com/api/get_block",
                 params={"bitrate": "4", "info":"true"})
r.raise_for_status()
newjsd = r.json()

out = {}

# If we are currently playing, check if our cached data is still
# relevant. If it is, it is more complete than the new data which is
# front-truncated, so we use it, so that we can check if we are still
# playing a track which might not be in the new list. Also if we go to
# a new block, we output the audio URL
if cached:
    debug("Cached end_event %s new %s"%(cached['end_event'], newjsd['end_event']))
if elapsedms >= 0 and cached and 'end_event' in cached and \
       cached['end_event'] == newjsd['end_event']:
    debug("rp-get-flac: using cached data")
    jsd = cached
else:
    debug("outputting audio url because using new metadata or not playing")
    jsd = newjsd
    elapsedms = -1
    newcache(jsd)
    
currentk = None
if elapsedms > 0:
    songs = jsd['song']
    for k in sorted(songs.keys()):
        startms = songs[k]['elapsed']
        endms  = startms + songs[k]['duration']
        debug("k %s Startms %d endms %d" % (k, startms, endms))
        if elapsedms >= startms and elapsedms <= endms:
            currentk = k
            break

if not currentk:
    # Not found ?? Try to reset the thing
    debug(("outputting audio url because current elapsed %d " + \
          " not found in song array") % elapsedms)
    jsd = newjsd
    newcache(jsd)
    out['reload'] = 3
else:
    songs = jsd['song']
    out['title'] = songs[currentk]['title']
    out['artist'] = songs[currentk]['artist']
    out['album'] = songs[currentk]['album']
    out['artUrl'] = 'http:%s%s' % (jsd['image_base'],
                                  songs[currentk]['cover'])
    reload = int((endms - elapsedms)/1000)
    # Last song: reload a bit earlier so that we can queue the URL
    if currentk == len(songs) -1 and reload > 3:
        reload -= 2
    out['reload'] = reload

debug("%s" % json.dumps(out))
print("%s" % json.dumps(out))
