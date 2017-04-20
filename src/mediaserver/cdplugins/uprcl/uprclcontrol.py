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

import bottle

from uprclutils import uplog
import uprclinit

# Note that we start a separate server. There might be a way to serve
# both the audio files and the control app from the same server...
def runbottle(host='0.0.0.0', port=9278):
    global datadir
    datadir = os.path.dirname(__file__)
    datadir = os.path.join(datadir, 'bottle')
    bottle.TEMPLATE_PATH = (os.path.join(datadir, 'views'),)
    bottle.run(host=host, port=port)


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
