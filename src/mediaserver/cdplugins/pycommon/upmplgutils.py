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
#
"""
Shared code for the tidal, qobuz, gmusic plugins.

   - Uses the interface for the entity objects (track, album...)
     concretely defined in models.py, but duck-typed.
   - Defines and uses the format for the permanent URLs

The module implements utility functions for translating to/from what
our parent expects or sends on the pipe.
"""
from __future__ import print_function, unicode_literals

import posixpath
import re
import sys

# Bogus class instanciated as global object for helping with reusing
# kodi addon code
class XbmcPlugin:
    SORT_METHOD_TRACKNUM = 1
    def __init__(self, idprefix):
        global g_idprefix
        self.entries = []
        self.objid = ''
        self.idprefix = idprefix
        g_idprefix = idprefix
    def addDirectoryItem(self, hdl, endpoint, title, isend):
        self.entries.append(direntry(self.idprefix + endpoint, self.objid, title))
        return
    def endOfDirectory(self, h):
        return
    def setContent(self, a, b):
        return
    def addSortMethod(self, a, b):
        return

default_mime = "audio/mpeg"
default_samplerate = "44100"

# For now, we pretend that all tracks have the same format (for the
# resource record). For some services this may not be true, we'll see
# if it can stay this way.
def setMimeAndSamplerate(m, s):
    global default_mime, default_samplerate
    default_mime = m
    default_samplerate = s
    
def trackentries(httphp, pathprefix, objid, tracks):
    """
    Transform a list of Track objects to the format expected by the parent

    Args:
        objid (str):  objid for the browsed object (the parent container)
        tracks is the array of Track objects to be translated
        tracks: a list of Track objects.
        
    Returns:
        A list of dicts, each representing an UPnP item, with the
        keys as expected in the plgwithslave.cxx resultToEntries() function. 

        The permanent URIs, are of the following form, based on the
        configured host:port and pathprefix arguments and track Id:

            http://host:port/pathprefix/track?version=1&trackId=<trackid>
    
    """
    global default_mime, default_samplerate
    
    entries = []
    for track in tracks:
        if not track.available:
            if 1:
                uplog("NOT AVAILABLE")
                try:
                    uplog("%s by %s" % (track.name, track.artist.name))
                except:
                    pass
            continue
        li = {}
        li['pid'] = objid
        li['id'] = objid + '$' + "%s" % track.id
        li['tt'] = track.name
        li['uri'] = 'http://%s' % httphp + \
                    posixpath.join(pathprefix,
                                   'track?version=1&trackId=%s' % track.id)
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
        li['upnp:class'] = track.upnpclass
        li['res:mime'] = default_mime
        li['res:samplefreq'] = default_samplerate
           
        entries.append(li)
    return entries

def trackid_from_urlpath(pathprefix, a):
    """
    Extract track id from a permanent URL path part.

    This supposes that the input URL has the format produced by the
    trackentries() method: <pathprefix>/track?version=1&trackId=<trackid>

    Args:
        pathprefix (str): our configured path prefix (e.g. /qobuz/)
        a (dict): the argument dict out of cmdtalk with a 'path' key
    Returns:
        str: the track Id.
    """
    
    if 'path' not in a:
        raise Exception("trackuri: no 'path' in args")
    path = a['path']

    # pathprefix + 'track?version=1&trackId=trackid
    exp = posixpath.join(pathprefix, '''track\?version=1&trackId=(.+)$''')
    m = re.match(exp, path)
    if m is None:
        raise Exception("trackuri: path [%s] does not match [%s]" % (path, exp))
    trackid = m.group(1)
    return trackid


def direntry(id, pid, title, arturi=None, artist=None, upnpclass=None):
    """ Create container entry in format expected by parent """
    ret = {'id':id, 'pid':pid, 'tt':title, 'tp':'ct', 'searchable':'1'}
    if arturi:
        ret['upnp:albumArtURI'] = arturi
    if artist:
        ret['upnp:artist'] = artist
    if upnpclass:
        ret['upnp:class'] = upnpclass
    return ret


def uplog(s):
    print("%s: %s" % (g_idprefix, s), file=sys.stderr)
