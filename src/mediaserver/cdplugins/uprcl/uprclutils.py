from __future__ import print_function

import sys

def rcldocs2entries(httphp, pathprefix, objid, docs):
    """
    Transform a list of Doc objects into the format expected by the parent

    Args:
        httphp: the hostport part of the generated track urls
        pathprefix: is provided by our parent process (it's used to
                    what plugin an url belongs too when needed for
          translating the internal into the real url (for plugins
          based on external-services)
        objid (str):  objid for the browsed object (the parent container)
        docs is the array of Doc objects to be translated
        
    Returns:
        A list of dicts, each representing an UPnP item, with the
        keys as expected in the plgwithslave.cxx resultToEntries() function. 

        The permanent URIs, are of the following form, based on the
        configured host:port and pathprefix arguments and track Id:

            http://host:port/pathprefix/track?version=1&trackId=<trackid>
    
    """
    global default_mime, default_samplerate
    
    entries = []
    for doc in docs:
        li = {}
        li['pid'] = objid
        li['id'] = objid + '$' + "%s" % track.id
        li['tt'] = doc.title

# URL: we need transformation rules from the file:// recoll url to an http
# one appropriate for our media server
        li['uri'] = 'http://%s' % httphp + \
                    posixpath.join(pathprefix,
                                   'track?version=1&trackId=%s' % track.id)
        li['tp'] = 'it'
        if doc.album:
            li['upnp:album'] = doc.album
        # !! Albumart will have to come from somewhere else !
###     #if doc.albumarturi:
###        #li['upnp:albumArtURI'] = track.album.image
        # Date format ?
        if doc.date:
            li['releasedate'] = doc.date
        li['upnp:originalTrackNumber'] =  str(doc.tracknumber)
        li['upnp:artist'] = doc.artist
        li['upnp:genre'] = doc.genre
        li['dc:title'] = doc.title
        li['upnp:class'] = track.upnpclass
        li['res:mime'] = doc.mtype
###     li['discnumber'] = str(track.disc_num)
### Need to extract the audio params from mutagen output !
        #li['duration'] = track.duration
        #li['res:samplefreq'] = default_samplerate
#albumartist=
#comment=
#composer=
#conductor=
#discnumber=
#genre=
#lyricist=
#lyrics=
           
        entries.append(li)
    return entries


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
