import subprocess
import sys
import bottle
import re
import time

SPLITRE = '''\|\|'''

def _listReceivers():
    devnull = open('/dev/null', 'w')
    try:
        data = subprocess.check_output(['scctl', '-lm'], stderr = devnull)
    except:
        data = "scctl error"
    o = []
    for line in data.splitlines():
        #print >> sys.stderr, line
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
    return o

@bottle.route('/static/:path#.+#')
def server_static(path):
    return bottle.static_file(path, root='./static')

@bottle.route('/')
@bottle.view('main')
def top():
    devnull = open('/dev/null', 'w')
    cmd = subprocess.Popen(['scctl', '-S'], stderr = devnull, stdout = devnull)
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
    devnull = open('/dev/null', 'w')

    assocs = bottle.request.forms.getall('Assoc')
    sender = bottle.request.forms.get('Sender')
    if sender != '' and len(assocs) != 0:
        arglist = ['scctl', '-r', sender]
        for uuid in assocs:
            arglist.append(uuid)
        print >> sys.stderr, arglist

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

    return {'receivers' : _listReceivers(), 'senders' : s}


@bottle.route('/stop')
@bottle.post('/stop')
@bottle.view('stop')
def stopReceivers():
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
    return {'active' : a}
