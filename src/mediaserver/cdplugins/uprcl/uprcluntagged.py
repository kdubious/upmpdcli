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

import os
import shlex
import urllib
import sys

from uprclutils import *

from recoll import recoll
from recoll import rclconfig

g_myprefix = '0$uprcl$untagged'


def recoll2untagged(docs):
    global g_utidx, g_rcldocs
    g_rcldocs = docs
    g_utidx = [-1]
    
    for docidx in range(len(docs)):
        doc = docs[docidx]
        if doc.mtype == 'inode/directory':
            continue
        url = doc.getbinurl()
        url = url[7:]
        try:
            decoded = url.decode('utf-8')
        except:
            decoded = urllib.quote(url).decode('utf-8')
        tt = doc.title
        if not tt:
            g_utidx.append(docidx)

def _objidtoidx(pid):
    if not pid.startswith(g_myprefix):
        raise Exception("untagged.browse: bad pid %s" % pid)

    if len(g_rcldocs) == 0:
        raise Exception("untagged:browse: no docs")

    idx = pid[len(g_myprefix):]
    if not idx:
        idx = 0
    else:
        if idx[1] != 'u':
            raise Exception("untagged:browse: called on bad objid %s" % pid)
        idx = int(idx[2:])
    
    if idx >= len(g_utidx):
        raise Exception("untagged:browse: bad pid %s" % pid)

    return idx


def rootentries(pid):
    return [rcldirentry(pid + 'untagged', pid, '[untagged]'),]

# Browse method
# objid is like untagged$*u<index>
# flag is meta or children. 
def browse(pid, flag, httphp, pathprefix):

    idx = _objidtoidx(pid)

    entries = []
    if idx == 0:
        # Browsing root
        for i in range(len(g_utidx))[1:]:
            doc = g_rcldocs[g_utidx[i]]
            id = g_myprefix + '$u' + str(i)
            e = rcldoctoentry(id, pid, httphp, pathprefix, doc)
            if e:
                entries.append(e)
    else:
        # Non root: only items in there. flag needs to be 'meta'
        doc = g_rcldocs[thisdocidx]
        id = g_myprefix + '$u' + str(idx)
        e = rcldoctoentry(id, pid, httphp, pathprefix, doc)
        if e:
            entries.append(e)

    return sorted(entries, cmp=cmpentries)

