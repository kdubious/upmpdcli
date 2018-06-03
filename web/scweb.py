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

from __future__ import print_function
import subprocess
import sys
import bottle
import re
import time

def _msg(s):
    print("%s"%s, file=sys.stderr)

cmd = None
def _maybestartserver():
    global cmd
    if not cmd:
        devnull = open('/dev/null', 'w')
        cmd = subprocess.Popen(['scctl', '-S'], stderr=devnull, stdout=devnull)
        devnull.close()
        
SPLITRE = b'''\|\|'''

def _listReceivers():
    _maybestartserver()
    devnull = open('/dev/null', 'w')
    try:
        data = subprocess.check_output(['scctl', '-lm'], stderr = devnull)
    except:
        data = "scctl error"
    o = []
    for line in data.splitlines():
        #_msg(line)
        fields = re.split(SPLITRE, line);
        if len(fields) == 4:
            status, fname, uuid, uri = fields
        elif len(fields) == 3:
            status, fname, uuid = fields
            uri = ''
        else:
            status = None
        if status:
            status = status.strip()
        if status is not None:
            o.append((fname, status, uuid, uri))
    devnull.close()
    return o

@bottle.route('/static/:path#.+#')
def server_static(path):
    return bottle.static_file(path, root='./static')

@bottle.route('/')
@bottle.view('main')
def top():
    _maybestartserver()
    # Sleep a wee bit to give a chance to the server to initialize
    time.sleep(1)
    return dict(title='')

@bottle.route('/list')
@bottle.view('list')
def listReceivers():
    return {'receivers' : _listReceivers()}

@bottle.route('/assoc')
@bottle.post('/assoc')
@bottle.view('assoc')
def assocReceivers():
    _maybestartserver()
    devnull = open('/dev/null', 'w')

    assocs = bottle.request.forms.getall('Assoc')
    sender = bottle.request.forms.get('Sender')
    if sender != '' and len(assocs) != 0:
        arglist = ['scctl', '-r', sender]
        for uuid in assocs:
            arglist.append(uuid)
        _msg(arglist)

        try:
            subprocess.check_call(arglist, stderr = devnull)
        except:
            pass

    try:
        data = subprocess.check_output(['scctl', '-Lm'], stderr = devnull)
    except:
        data = "scctl error"

    s = []
    for line in data.splitlines():
        fields = re.split(SPLITRE, line);
        fname, uuid, reason, uri = fields
        s.append((fname, uuid, uri))
    devnull.close()
    return {'receivers' : _listReceivers(), 'senders' : s}


@bottle.route('/stop')
@bottle.post('/stop')
@bottle.view('stop')
def stopReceivers():
    _maybestartserver()
    devnull = open('/dev/null', 'w')
    for uuid in bottle.request.forms.getall('Stop'):
        try:
            subprocess.check_call(['scctl', '-x', uuid], stderr=devnull)
        except:
            pass

    try:
        data = subprocess.check_output(['scctl', '-lm'], stderr = devnull)
    except:
        data = "scctl error"

    a = []
    for line in data.splitlines():
        fields = re.split(SPLITRE, line);
        if len(fields) == 4:
            status, fname, uuid, uri = fields
            if status != 'Off':
                a.append((fname, status, uuid, uri))
        elif len(fields) == 3:
            status, fname, uuid = fields
            if status != 'Off':
                a.append(fname, status, uuid, '')
    devnull.close()
    return {'active' : a}
