from __future__ import print_function

import sys
import posixpath
import urllib

audiomtypes = frozenset([
    'audio/mpeg',
    'application/x-flac',
    'application/ogg',
    'audio/aac',
    'audio/mp4',
    'audio/x-aiff',
    'audio/x-wav'
    ])

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

        The permanent URIs, are of the following form, based on the
        configured host:port and pathprefix arguments and track Id:
        TBD
            http://host:port/pathprefix/track?version=1&trackId=<trackid>
    
    """
    uplog("rcldoctoentry:  pid %s id %s httphp %s pathprefix %s" %
          (pid, id, httphp, pathprefix))
    
    li = {}
    if doc.mtype not in audiomtypes:
        return li

    li['pid'] = pid
    li['id'] = id
    li['tp'] = 'it'
    # Why no dc.title??
    li['tt'] = doc.title

    # TBD
    li['upnp:class'] = 'object.item.audioItem.musicTrack'

    # TBD Date format ?
    # !! Albumart will have to come from somewhere else !
    # li['upnp:class'] = doc.upnpclass
    #li['res:channels'] =
    #li['res:size'] =
    #li['res:bitrate'] = 
    ###     #if doc.albumarturi:
    ###        #li['upnp:albumArtURI'] = track.album.image
    ### li['discnumber'] = str(track.disc_num)
    #albumartist=
    #comment=
    #composer=
    #conductor=
    #discnumber=
    #genre=
    #lyricist=
    #lyrics=

    for oname,dname in [('upnp:album', 'album'), ('releasedate','date'),
                        ('upnp:originalTrackNumber', 'tracknumber'),
                        ('upnp:artist', 'artist'), ('upnp:genre', 'genre'),
                        ('res:mime', 'mtype'), ('duration', 'duration'),
                        ('res:samplefreq', 'sample_rate')]:
        val = getattr(doc, dname)
        if val:
            li[oname] = val

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
    path = pathprefix + path
    li['uri'] = "http://%s%s" % (httphp, urllib.quote(path))
    uplog("rcldoctoentry: uri: %s" % li['uri'])
    return li

def rclpathtoreal(path, pathprefix, httphp, pathmap):
    path = path.replace(pathprefix, '', 1)
    found = False
    for fsp,htp in pathmap.iteritems():
        if path.startswith(fsp):
            path = path.replace(fsp, htp, 1)
            found = True
    if not found:
        return None
    return "http://" + httphp + path

def rcldirentry(id, pid, title, arturi=None, artist=None, upnpclass=None):
    """ Create container entry in format expected by parent """
    ret = {'id':id, 'pid':pid, 'tt':title, 'tp':'ct', 'searchable':'1'}
    if arturi:
        ret['upnp:albumArtURI'] = arturi
    if artist:
        ret['upnp:artist'] = artist
    if upnpclass:
        ret['upnp:class'] = upnpclass
    else:
        ret['upnp:class'] = 'object.container'
    return ret

def uplog(s):
    print(("%s: %s" % ('uprcl', s)).encode('utf-8'), file=sys.stderr)
