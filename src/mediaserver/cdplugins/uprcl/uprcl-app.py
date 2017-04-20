#!/usr/bin/env python
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

import sys
import os
import json
import re
import cmdtalkplugin

import uprclfolders
import uprcltags
import uprcluntagged
import uprclsearch
import uprclhttp
import uprclindex

from uprclutils import uplog, g_myprefix, rcldirentry
from uprclinit import g_pathprefix, g_httphp, g_dblock, g_rclconfdir
import uprclinit

#####
# Initialize communication with our parent process: pipe and method
# call dispatch
# Func name to method mapper
dispatcher = cmdtalkplugin.Dispatch()
# Pipe message handler
msgproc = cmdtalkplugin.Processor(dispatcher)

@dispatcher.record('trackuri')
def trackuri(a):
    # This is used for plugins which generate temporary local urls
    # pointing to the microhttpd instance. The microhttpd
    # answer_to_connection() routine in plgwithslave calls 'trackuri'
    # to get a real URI to redirect to. We generate URIs which
    # directly point to our python http server, so this method should
    # never be called.
    msgproc.log("trackuri: [%s]" % a)
    raise Exception("trackuri: should not be called for uprcl!")

# objid prefix to module map
rootmap = {}

def _rootentries():
    # Build up root directory. This is our top internal structure. We
    # let the different modules return their stuff, and we take note
    # of the objid prefixes for later dispatching
    entries = []

    nents = uprcltags.rootentries(g_myprefix)
    for e in nents:
        rootmap[e['id']] = 'tags'
    entries += nents

    nents = uprcluntagged.rootentries(g_myprefix)
    for e in nents:
        rootmap[e['id']] = 'untagged'
    entries += nents

    nents = uprclfolders.rootentries(g_myprefix)
    for e in nents:
        rootmap[e['id']] = 'folders'
    entries += nents

    uplog("Browse root: rootmap now %s" % rootmap)
    return entries

def _browsedispatch(objid, bflg, httphp, pathprefix):
    for id,mod in rootmap.iteritems():
        #uplog("Testing %s against %s" % (objid, id))
        if objid.startswith(id):
            if mod == 'folders':
                return uprclfolders.browse(objid, bflg, httphp, pathprefix)
            elif mod == 'tags':
                return uprcltags.browse(objid, bflg, httphp, pathprefix)
            elif mod == 'untagged':
                return uprcluntagged.browse(objid, bflg, httphp, pathprefix)
            else:
                raise Exception("Browse: dispatch: bad mod " + mod)
    raise Exception("Browse: dispatch: bad objid not in rootmap: " + objid)

@dispatcher.record('browse')
def browse(a):
    msgproc.log("browse: %s" % a)
    if 'objid' not in a:
        raise Exception("No objid in args")

    objid = a['objid']
    bflg = a['flag'] if 'flag' in a else 'children'
    
    if not objid.startswith(g_myprefix):
        raise Exception("bad objid <%s>" % objid)

    idpath = objid.replace(g_myprefix, '', 1)
    msgproc.log("browse: idpath: <%s>" % idpath)


    entries = []
    nocache = "0"
    if bflg == 'meta':
        raise Exception("uprcl-app: browse: can't browse meta for now")
    else:
        try:
            if not uprclinit.ready():
                entries = [rcldirentry(objid + 'notready', objid,
                                       'Initializing...'),]
                nocache = "1"
            elif not idpath:
                entries = _rootentries()
            else:
                entries = _browsedispatch(objid, bflg, g_httphp, g_pathprefix)
        finally:
            g_dblock.release_read()

    #msgproc.log("%s" % entries)
    encoded = json.dumps(entries)
    return {"entries" : encoded, "nocache":nocache}


@dispatcher.record('search')
def search(a):
    msgproc.log("search: [%s]" % a)
    objid = a['objid']
    if re.match('0\$uprcl\$', objid) is None:
        raise Exception("bad objid [%s]" % objid)

    upnps = a['origsearch']
    nocache = "0"

    try:
        if not uprclinit.ready():
            entries = [rcldirentry(objid + 'notready', objid,
                                   'Initializing...'),]
            nocache = "1"
        else:
            entries = uprclsearch.search(g_rclconfdir, objid, upnps, g_myprefix,
                                         g_httphp, g_pathprefix)
    finally:
        g_dblock.release_read()

    encoded = json.dumps(entries)
    return {"entries" : encoded, "nocache":nocache}


uprclinit.uprcl_init()
msgproc.log("Uprcl running")
msgproc.mainloop()
