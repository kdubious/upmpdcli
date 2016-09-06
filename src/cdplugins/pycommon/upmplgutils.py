# # de shared by the tidal, qobuz and gmusic plugins. This assumes a
# certain formats for track, albums, and artist objects (as defined in
# practise in models.py, but if it quacks...), and also for permanent
# URLs, and implements utility functions for translating to what our
# parent expects or sends on the pipe.
from __future__ import print_function, unicode_literals

import posixpath
import re

# Transform a Track array into an array of dicts suitable for
# returning as a track list
def trackentries(httphp, pathprefix, objid, tracks):
    entries = []
    for track in tracks:
        if not track.available:
            continue
        li = {}
        li['pid'] = objid
        li['id'] = objid + '$' + "%s" % track.id
        li['tt'] = track.name
        li['uri'] = 'http://%s' % httphp + \
                    posixpath.join(pathprefix,
                                   'track?version=1&trackId=%s' % \
                                   track.id)
        #msgproc.log("URI: [%s]" % li['uri'])
        li['tp'] = 'it'
        if track.album:
            li['upnp:album'] = track.album.name
            if track.album.image:
                li['upnp:albumArtURI'] = track.album.image
            if track.album.release_date:
                li['releasedate'] = track.album.release_date 
        li['upnp:originalTrackNumber'] =  str(track.track_num)
        li['upnp:artist'] = track.artist.name
        li['dc:title'] = track.name
        li['discnumber'] = str(track.disc_num)
        li['duration'] = track.duration
        entries.append(li)
    return entries

# Container entry
def direntry(id, pid, title):
    return {'id': id, 'pid' : pid, 'tt': title, 'tp':'ct', 'searchable' : '1'}

# Directory entries from list of (name,id) pairs
def direntries(objid, ttidlist):
    content = []
    for tt,id in ttidlist:
        content.append(direntry(objid + '$' + id, objid, tt))
    return content

# Extract trackid from URL. Supposes that our permanent URLs look like
# track?version=1&trackId=<trackid>
def trackid_from_urlpath(pathprefix, a):
    if 'path' not in a:
        raise Exception("No path in args")
    path = a['path']

    # pathprefix + 'track?version=1&trackId=trackid
    exp = posixpath.join(pathprefix, '''track\?version=1&trackId=(.+)$''')
    m = re.match(exp, path)
    if m is None:
        raise Exception("trackuri: path [%s] does not match [%s]" % (path, exp))
    trackid = m.group(1)
    return trackid

