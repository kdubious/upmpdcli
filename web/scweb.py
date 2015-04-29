import subprocess
import sys
import bottle
import re
import time

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
    devnull = open('/dev/null', 'w')
    try:
        data = subprocess.check_output(['scctl', '-l'], stderr = devnull)
    except:
        data = "scctl error"
    o = []
    for line in data.splitlines():
        #print >> sys.stderr, line
        fields = re.split('''\s+''', line);
        if len(fields) == 4:
            status, fname, uuid, uri = fields
        elif len(fields) == 3:
            status, fname, uuid = fields
            uri = ''
        else:
            status = None
        if status is not None:
            o.append((fname, status, uuid, uri))
    return {'receivers' : o}

@bottle.route('/assoc')
@bottle.post('/assoc')
@bottle.view('assoc')
def assocReceivers():
    devnull = open('/dev/null', 'w')

    assocs = bottle.request.forms.getall('Assoc')
    master = bottle.request.forms.get('Master')
    if master != '' and len(assocs) != 0:
        arglist = ['scctl', '-s', master]
        for uuid in assocs:
            arglist.append(uuid)
            try:
                subprocess.check_call(arglist, stderr = devnull)
            except:
                pass

    try:
        data = subprocess.check_output(['scctl', '-l'], stderr = devnull)
    except:
        data = "scctl error"

    a = []
    o = []
    for line in data.splitlines():
        fields = re.split('''\s+''', line);
        if len(fields) == 4:
            status, fname, uuid, uri = fields
            if status != 'Off' and uri != '':
                a.append((fname, status, uuid, uri))
            else:
                o.append((fname, status, uuid, uri))
        elif len(fields) == 3:
            status, fname, uuid = fields
            o.append(fname, status, uuid, '')
    return {'active' : a, 'others' : o}


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
        data = subprocess.check_output(['scctl', '-l'], stderr = devnull)
    except:
        data = "scctl error"

    a = []
    for line in data.splitlines():
        fields = re.split('''\s+''', line);
        if len(fields) == 4:
            status, fname, uuid, uri = fields
            if status != 'Off':
                a.append((fname, status, uuid, uri))
        elif len(fields) == 3:
            status, fname, uuid = fields
            if status != 'Off':
                a.append(fname, status, uuid, '')
    return {'active' : a}
