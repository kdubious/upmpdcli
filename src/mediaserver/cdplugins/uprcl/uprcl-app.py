#!/usr/bin/env python
#
# Copyright (C) 2016 J.F.Dockes
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
import os
import json
import posixpath
import re
import conftree
import cmdtalkplugin
import urllib

import uprclfolders
import uprclsearch
from uprclutils import *

from recoll import recoll
from recoll import rclconfig
g_myprefix = '0$uprcl$folders'

# Func name to method mapper
dispatcher = cmdtalkplugin.Dispatch()
# Pipe message handler
msgproc = cmdtalkplugin.Processor(dispatcher)

def uprcl_init():
    global httphp, pathprefix, uprclhost, pathmap, rclconfdir
    
    if "UPMPD_HTTPHOSTPORT" not in os.environ:
        raise Exception("No UPMPD_HTTPHOSTPORT in environment")
    httphp = os.environ["UPMPD_HTTPHOSTPORT"]
    if "UPMPD_PATHPREFIX" not in os.environ:
        raise Exception("No UPMPD_PATHPREFIX in environment")
    pathprefix = os.environ["UPMPD_PATHPREFIX"]
    if "UPMPD_CONFIG" not in os.environ:
        raise Exception("No UPMPD_CONFIG in environment")
    upconfig = conftree.ConfSimple(os.environ["UPMPD_CONFIG"])

    uprclhost = upconfig.get("uprclhost")
    if uprclhost is None:
        raise Exception("uprclhost not in config")

    pthstr = upconfig.get("uprclpaths")
    if pthstr is None:
        raise Exception("uprclpaths not in config")
    lpth = pthstr.split(',')
    pathmap = {}
    for ptt in lpth:
        l = ptt.split(':')
        pathmap[l[0]] = l[1]

    global rclconfdir
    rclconfdir = upconfig.get("uprclconfdir")
    if rclconfdir is None:
        raise Exception("uprclconfdir not in config")

    uprclfolders.inittree(rclconfdir)


@dispatcher.record('trackuri')
def trackuri(a):
    msgproc.log("trackuri: [%s]" % a)
    if 'path' not in a:
        raise Exception("trackuri: no 'path' in args")
    path = urllib.quote(a['path'])
    media_url = rclpathtoreal(path, pathprefix, uprclhost, pathmap)
    msgproc.log("trackuri: returning: %s" % media_url)
    return {'media_url' : media_url}


@dispatcher.record('browse')
def browse(a):
    msgproc.log("browse: [%s]" % a)
    if 'objid' not in a:
        raise Exception("No objid in args")
    objid = a['objid']
    bflg = a['flag'] if 'flag' in a else 'children'
    
    if re.match('0\$uprcl\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)

    idpath = objid.replace('0$uprcl$', '', 1)
    msgproc.log("browse: idpath: %s" % idpath)

    entries = []

    if bflg == 'meta':
        m = re.match('.*\$(.+)$', idpath)
        if m:
            trackid = m.group(1)
            # get doc from trackid or whatever
            doc = {}
            entries += trackentries(httphp, pathprefix, objid, [])
    else:
        if not idpath:
            # Build up root directory. No external data needed, this is our
            # top internal structure
            entries.append(rcldirentry('0$uprcl$' + 'folders', '0$uprcl$',
                                        '[folders]'))
        elif idpath.startswith("folders"):
            entries = uprclfolders.browse(objid, bflg, httphp, pathprefix)
        else:
            pass


    #msgproc.log("%s" % entries)
    encoded = json.dumps(entries)
    return {"entries" : encoded}


@dispatcher.record('search')
def search(a):
    msgproc.log("search: [%s]" % a)
    objid = a['objid']
    if re.match('0\$uprcl\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)

    upnps = a['origsearch']

    entries = uprclsearch.search(rclconfdir, objid, upnps, g_myprefix,
                                 httphp, pathprefix)
    
    encoded = json.dumps(entries)
    return {"entries" : encoded}

uprcl_init()

msgproc.log("Uprcl running")
msgproc.mainloop()
