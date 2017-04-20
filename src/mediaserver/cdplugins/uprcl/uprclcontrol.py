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

from bottle import route, run, template
from bottle import static_file

from uprclutils import uplog
import uprclinit

# Note that we start a separate server. There might be a way to serve
# both the audio files and the control app from the same server...
def runbottle(host='0.0.0.0', port=9278):
    global datadir
    datadir = os.path.dirname(__file__)
    datadir = os.path.join(datadir, 'bottle')
    run(host=host, port=port)


@route('/')
def main(name='namesample'):
    return template('<b>MAIN PAGE for {{name}}</b>!', name=name)

@route('/update')
def main(name='namesample'):
    # Can't use import directly because of the minus sign.
    uplog("Calling start_update")
    uprclinit.start_update()
    uplog("start_update returned")
    return template('<b>Index update started {{name}}</b>!', name='')

@route('/static/<filepath:path>')
def server_static(filepath):
    uplog("control: server_static: filepath %s datadir %s" % (filepath,datadir))
    return static_file(filepath, root=os.path.join(datadir, 'static'))

@route('/hello/<name>')
def index(name):
    return template('<b>Hello {{name}}</b>!', name=name)

