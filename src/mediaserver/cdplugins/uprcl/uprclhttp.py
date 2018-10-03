# Copyright (C) 2017 J.F.Dockes
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

from __future__ import print_function

import os
import time
import bottle
import mutagen

from upmplgutils import uplog
from uprclutils import embedded_open
import uprclinit

@bottle.route('/')
@bottle.post('/')
@bottle.view('main')
def main():
    sub =  bottle.request.forms.get('sub')
    #uplog("Main: sub value is %s" % sub)
    if uprclinit.updaterunning():
        status = 'Updating'
    else:
        status = 'Ready'

    if sub == 'Update Index':
        uprclinit.start_update()

    if sub:
        headers = dict()
        headers["Location"] = '/'
        return bottle.HTTPResponse(status=302, **headers)
    else:
        return {'title':status, 'status':status,
                'friendlyname':uprclinit.g_friendlyname}

@bottle.route('/static/<filepath:path>')
def static(filepath):
    #uplog("control: static: filepath %s datadir %s" % (filepath, datadir))
    return bottle.static_file(filepath, root=os.path.join(datadir, 'static'))


# Object for streaming data from a given subtree (topdirs entry more
# or less). This is needed just because as far as I can see, a
# callback can't know the route it was called for, so we record it
# when creating the object.
class Streamer(object):
    def __init__(self, root):
        self.root = root

    def __call__(self, filepath):
        embedded = True if 'embed' in bottle.request.query else False
        if embedded:
            # Embedded image urls have had a .jpg or .png
            # appended. Remove it to restore the track path name.
            i = filepath.rfind('.')
            filepath = filepath[:i]
            apath = os.path.join(self.root,filepath)
            ctype, size, f = embedded_open(apath)
            fs = os.stat(apath)
            lm = time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(fs.st_mtime))
            bottle.response.set_header("Last-Modified", lm)
            bottle.response.set_header("Content-type", ctype)
            bottle.response.set_header("Content-Length", size)
            return f
        fullpath = os.path.join(self.root, filepath)
        uplog("Streaming: %s " % fullpath)
        mutf = mutagen.File(fullpath)
        if mutf:
            return bottle.static_file(filepath, root=self.root,
                                      mimetype=mutf.mime[0])
        else:
            return bottle.static_file(filepath, root=self.root)
    

# Bottle handle both the streaming and control requests.
def runbottle(host='0.0.0.0', port=9278, pthstr='', pathprefix=''):
    global datadir
    uplog("runbottle: host %s port %d pthstr %s pathprefix %s" %
          (host, port, pthstr, pathprefix))
    datadir = os.path.dirname(__file__)
    datadir = os.path.join(datadir, 'bottle')
    bottle.TEMPLATE_PATH = (os.path.join(datadir, 'views'),)

    # All the file urls must be like /some/prefix/path where
    # /some/prefix must be in the path translation map (which I'm not
    # sure what the use of is). By default the map is an identical
    # translation of all topdirs entries. We create one route for each
    # prefix. As I don't know how a bottle method can retrieve the
    # route it was called from, we create a callable for each prefix.
    # Each route is built on the translation input, and the processor
    # uses the translated path as root
    lpth = pthstr.split(',')
    for ptt in lpth:
        l = ptt.split(':')
        rt = l[0]
        if rt[-1] != '/':
            rt += '/'
        rt += '<filepath:path>'
        uplog("runbottle: adding route for: %s"%rt)
        # We build the streamer with the translated 
        streamer = Streamer(l[1])
        bottle.route(rt, 'GET', streamer)

    bottle.run(server='waitress', host=host, port=port)
