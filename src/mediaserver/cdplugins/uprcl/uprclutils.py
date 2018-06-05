#
# Copyright (C) 2017 J.F.Dockes
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
from __future__ import print_function

import sys
PY3 = sys.version > '3'
if PY3:
    from urllib.parse import quote as urlquote
    import functools
else:
    from urllib import quote as urlquote
    
import os
import glob
import subprocess
import mutagen
import io

from upmplgutils import uplog

# This must be consistent with what contentdirectory.cxx does
g_myprefix = '0$uprcl$'

audiomtypes = frozenset([
    'audio/mpeg',
    'audio/flac',
    'application/flac',
    'audio/x-flac',
    'application/x-flac',
    'application/ogg',
    'audio/aac',
    'audio/mp4',
    'video/mp4',
    'audio/x-aiff',
    'audio/x-musepack',
    'audio/ape',
    'audio/x-wav',
    'audio/x-wavpack',
    'inode/directory'
    ])

# Correspondance between Recoll field names (on the right), defined by
# rclaudio and the Recoll configuration 'fields' file, and what
# plgwithslave.cxx expects, which is less than consistent.
upnp2rclfields = {
    'upnp:album': 'album',
    'upnp:artist' : 'artist',
    'comment' : 'comment',
    'composer' : 'composer',
    'conductor' : 'conductor',
    'dc:date' : 'date',
    'upnp:genre' : 'genre',
    'duration' : 'duration', #should be res:
    'res:bitrate' : 'bitrate',
    'res:channels' : 'channels',
    'res:mime' : 'mtype',
    'res:samplefreq' : 'sample_rate',
    'res:size' : 'fbytes',
    'tt' : 'title',
    'upnp:originalTrackNumber' : 'tracknumber',
    }

def rcldoctoentry(id, pid, httphp, pathprefix, doc):
    """
    Transform a Doc objects into the format expected by the parent

    Args:
        id (str): objid for the entry
        pid (str):  objid for the browsed object (the parent container)
        httphp: the hostport part of the generated track urls
        pathprefix: is provided by our parent process (it's used to
          what plugin an url belongs too when needed for
          translating the internal into the real url (for plugins
          based on external-services)
        doc is the Doc object to be translated
        
    Returns:
        A dict representing an UPnP item, with the
        keys as expected in the plgwithslave.cxx resultToEntries() function. 
    """
    #uplog("rcldoctoentry:  pid %s id %s mtype %s" % (pid, id, doc.mtype))
    
    li = {}
    if doc.mtype not in audiomtypes:
        return li

    li['pid'] = pid
    li['id'] = id
    if doc.mtype == 'inode/directory':
        li['tp'] = 'ct'
        li['upnp:class'] = 'object.container'
    else:
        li['tp']= 'it'
        # TBD
        li['upnp:class'] = 'object.item.audioItem.musicTrack'

    for oname,dname in upnp2rclfields.items():
        val = getattr(doc, dname)
        if val:
            li[oname] = val

    if 'upnp:artist' not in li and doc.albumartist:
        li['upnp:artist'] = doc.albumartist

    # TBD Date format ?
    #comment=
    #composer=
    #conductor=
    #discnumber=
    #genre=
    #lyricist=
    #lyrics=

    try:
        val = li['upnp:originalTrackNumber']
        l = val.split('/')
        li['upnp:originalTrackNumber'] = l[0]
    except:
        pass
        
    # Compute the url. We use the URL from recoll, stripped of file://
    # and with the pathprefix prepended (the pathprefix is used by our
    # parent process to match urls to plugins)
    path = doc.getbinurl()
    path = path[7:]
    if 'tt' not in li:
        li['tt'] = os.path.basename(path.decode('UTF-8', errors = 'replace'))
    path = os.path.join(pathprefix.encode('ascii'), path)
    li['uri'] = "http://%s%s" % (httphp, urlquote(path))
    #uplog("rcldoctoentry: uri: %s" % li['uri'])

    # The album art uri is precooked with httphp and prefix
    if doc.albumarturi:
        li['upnp:albumArtURI'] = doc.albumarturi
        #uplog("Set upnp:albumArtURI to %s" % li['upnp:albumArtURI'])

    return li

# Bogus entry for the top directory while the index/db is updating
def waitentry(id, pid, httphp):
    li = {}
    li['tp'] = 'it'
    li['id'] = id
    li['pid'] = pid
    li['upnp:class'] = 'object.item.audioItem.musicTrack'
    li['tt'] = "Initializing..."
    li['uri'] = "http://%s%s" % (httphp, "/waiting")
    li['res.mime'] = "audio/mpeg"
    return li


# Compute fs path for URL. All Recoll URLs are like file://xx
def docpath(doc):
    return doc.getbinurl()[7:]

def docfolder(doc):
    path = docpath(doc)
    if doc.mtype == 'inode/directory':
        return path
    else:
        return os.path.dirname(path)

def embdimgurl(doc, httphp, pathprefix):
    if doc.embdimg == 'jpg':
        ext = b'.jpg'
    elif doc.embdimg == '.png':
        ext = b'png'
    else:
        return None
    path = doc.getbinurl()
    path = path[7:]
    path = urlquote(os.path.join(pathprefix.encode('ascii'), path+ext))
    path +=  "?embed=1"
    return "http://%s%s" % (httphp, path)

def printable(s):
    return s.decode('utf-8', errors='replace') if s else ""

def _httpurl(path, httphp, pathprefix):
    return "http://%s%s" % (httphp, urlquote(path))
    
# Find cover art for doc.
#
# We return a special uri if the file has embedded image data, else an
# uri for for the directory cover art (if any).
# We are usually called repeatedly for the same directory, so we cache
# one result.
_foldercache = {}
_artexts = (b'.jpg', b'.png')
_artnames = ('folder', 'cover')
def docarturi(doc, httphp, pathprefix):
    global _foldercache, _artnames

    if doc.embdimg:
        arturi = embdimgurl(doc, httphp, pathprefix)
        if arturi:
            #uplog("docarturi: embedded: %s"%printable(arturi))
            return arturi

    # Check for an image specific to the track file
    path,ext = os.path.splitext(docpath(doc))
    for ext in _artexts:
        if os.path.exists(path + ext):
            return _httpurl(os.path.join(pathprefix.encode('ascii'),
                                         path+ext), httphp, pathprefix)

    # If doc is a directory, this returns itself, else the father dir.
    folder = docfolder(doc)

    if folder not in _foldercache:
        _foldercache = {}
        _foldercache[folder] = None
        artnm = None
        try:
            for f in os.listdir(folder):
                try:
                    fsimple = os.path.basename(f)
                    flowersimple = fsimple.lower()
                except:
                    continue
                for base in _artnames:
                    for ext in _artexts:
                        if flowersimple == base + ext:
                            artnm = fsimple
                if artnm:
                    _foldercache[folder] = _httpurl(
                        urlquote(os.path.join(pathprefix, folder, artnm)),
                        httphp, pathprefix)
                    break
        except:
            pass

    arturi = _foldercache[folder]
    if arturi:
        if doc.mtype == 'inode/directory':
            #uplog("docarturi: external: %s->%s" %
            #      (printable(folder), printable(arturi)))
            pass
    return arturi

def _keyvalornull(a, k):
    return a[k] if k in a else "NULL"
def _logentry(nm, e1):
    tp = _keyvalornull(e1,'tp')
    al = _keyvalornull(e1, 'upnp:album')
    dr = os.path.dirname(_keyvalornull(e1, 'uri'))
    tn = _keyvalornull(e1, 'upnp:originalTrackNumber')
    uplog("%s tp %s alb %s dir %s tno %s" % (nm, tp,al,dr,tn))

def _cmpentries_func(e1, e2):
    #uplog("cmpentries");_logentry("e1", e1);_logentry("e2", e2)
    tp1 = e1['tp']
    tp2 = e2['tp']
    isct1 = tp1 == 'ct'
    isct2 = tp2 == 'ct'

    # Containers come before items, and are sorted in alphabetic order
    if isct1 and  not isct2:
        return 1
    elif not isct1 and isct2:
        return -1
    elif isct1 and isct2:
        tt1 = e1['tt']
        tt2 = e2['tt']
        if tt1.lower() < tt2.lower():
            return -1
        elif tt1.lower() > tt2.lower():
            return 1
        else:
            return 0

    # Tracks. Sort by album then directory then track number
    k = 'upnp:album'
    a1 = e1[k] if k in e1 else ""
    a2 = e2[k] if k in e2 else ""
    if a1 < a2:
        return -1
    elif a1 > a2:
        return 1

    d1 = os.path.dirname(e1['uri'])
    d2 = os.path.dirname(e2['uri'])
    if d1 < d2:
        return -1
    elif d1 > d2:
        return 1
    
    k = 'upnp:originalTrackNumber'
    a1 = e1[k] if k in e1 else "0"
    a2 = e2[k] if k in e2 else "0"
    try:
        return int(a1) - int(a2)
    except:
        uplog("upnp:originalTrackNumber %s %s"% (a1, a2))
        return 0

if PY3:
    cmpentries=functools.cmp_to_key(_cmpentries_func)
else:
    cmpentries=_cmpentries_func

def rcldirentry(id, pid, title, arturi=None, artist=None, upnpclass=None,
                searchable='1', date=None):
    """ Create container entry in format expected by parent """
    #uplog("rcldirentry: id %s pid %s tt %s dte %s clss %s artist %s arturi %s" %
    #      (id,pid,title,date,upnpclass,artist,arturi))
    ret = {'id':id, 'pid':pid, 'tt':title, 'tp':'ct', 'searchable':searchable}
    if arturi:
        ret['upnp:albumArtURI'] = arturi
    if artist:
        ret['upnp:artist'] = artist
    if date:
        ret['dc:date'] = date
    if upnpclass:
        ret['upnp:class'] = upnpclass
    else:
        ret['upnp:class'] = 'object.container'
    return ret


# Parse string into (possibly multiword) tokens
# 'a b "one phrase" c' -> [a, b, 'one phrase', c]
def stringToStrings(str):
    # States. Note that ESCAPE can only occur inside INQUOTE
    SPACE, TOKEN, INQUOTE, ESCAPE = range(4)
    
    tokens = []
    curtok = ""
    state = SPACE;

    for c in str:
        if c == '"':
            if state == SPACE:
                state = INQUOTE
            elif state == TOKEN:
                curtok += '"'
            elif state == INQUOTE:
                if curtok:
                    tokens.append(curtok);
                curtok = ""
                state = SPACE
            elif state == ESCAPE:
                curtok += '"'
                state = INQUOTE
            continue;

        elif c == '\\':
            if state == SPACE or state == TOKEN:
                curtok += '\\'
                state = TOKEN
            elif state == INQUOTE:
                state = ESCAPE
            elif state == ESCAPE:
                curtok += '\\'
                state = INQUOTE
            continue

        elif c == ' ' or c == '\t' or c == '\n' or c == '\r':
            if state == SPACE or state == TOKEN:
                if curtok:
                    tokens.append(curtok)
                curtok = ""
                state = SPACE
            elif state == INQUOTE or state == ESCAPE:
                curtok += c
            continue;

        else:
            if state == ESCAPE:
                state = INQUOTE
            elif state == SPACE:
                state = TOKEN
            elif state == TOKEN or state == INQUOTE:
                pass
            curtok += c

    if state == SPACE:
        pass
    elif state == TOKEN:
        if curtok:
            tokens.append(curtok)
    elif state == INQUOTE or state == ESCAPE:
        raise Exception("Bad string: <" + str + ">")

    return tokens


# Find first non loopback ip. This is surprisingly
# difficult. Executing "ip addr" actually seems to be the simplest
# approach, only works on Linux though (maybe bsd too ?)
def findmyip():
    data = subprocess.check_output(["ip", "addr"])
    l = data.split()
    ips = []
    chosenip = ""
    for i in range(len(l)):
        k = l[i]
        if k == 'inet':
            ipmask = l[i+1]
            if ipmask.find('127.') == 0:
                continue
            return ipmask.split('/')[0]
            

# Open embedded image. Returns mtype, size, f
def embedded_open(path):
    try:
        mutf = mutagen.File(path)
    except Exception as err:
        raise err
        
    f = None
    size = 0
    if 'audio/mp3' in mutf.mime:
        for tagname in mutf.keys():
            if tagname.startswith('APIC:'):
                #self.em.rclog("mp3 img: %s" % mutf[tagname].mime)
                mtype = mutf[tagname].mime
                s = mutf[tagname].data
                size = len(s)
                f = io.BytesIO(s)
    elif 'audio/x-flac' in mutf.mime:
        if mutf.pictures:
            mtype = mutf.pictures[0].mime
            size = len(mutf.pictures[0].data)
            f = io.BytesIO(mutf.pictures[0].data)
    elif 'audio/mp4' in mutf.mime:
        if 'covr' in mutf.keys():
            format = mutf['covr'][0].imageformat 
            if format == mutagen.mp4.AtomDataType.JPEG:
                mtype = 'image/jpeg'
            else:
                mtype = 'image/png'
            size = len(mutf['covr'][0])
            f = io.BytesIO(mutf['covr'][0])

    if f is None:
        raise Exception("can't open embedded image")
    else:
        return mtype, size, f
