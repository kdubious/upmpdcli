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

# Manage the 'untagged' section of the tree.  Our selection is made of
# all tracks without a 'title' field. This is different from what
# Minimserver does (I think that more or less any field present makes
# a track not untagged for minim).
#
# Initialization filters the untagged tracks and creates a vector of
# indexes into the global doc vector.
#
# Object Id prefix: 0$uprcl$untagged
# 
# Obect id inside the section: $u<idx> where <idx> is the document index
#  inside the global document vector.

import os
import shlex
import urllib
import sys

from upmplgutils import uplog
from uprclutils import rcldoctoentry, rcldirentry

untg_prefix = '0$uprcl$untagged'

# Create the untagged entries static vector by filtering the global
# doc vector, storing the indexes of all tracks without a title
# field. We keep a reference to the doc vector.
def recoll2untagged(docs):
    global g_utidx, g_rcldocs
    g_rcldocs = docs
    # The -1 entry is because we use index 0 for our root.
    g_utidx = [-1]
    
    for docidx in range(len(docs)):
        doc = docs[docidx]
        if doc.mtype == 'inode/directory':
            continue
        if not doc.title:
            g_utidx.append(docidx)


# Compute index into our entries vector by 'parsing' the objid.
def _objidtoidx(pid):
    if not pid.startswith(untg_prefix):
        raise Exception("untagged.browse: bad pid %s" % pid)

    if len(g_rcldocs) == 0:
        raise Exception("untagged:browse: no docs")

    idx = pid[len(untg_prefix):]
    if not idx:
        # Browsing the root.
        idx = 0
    else:
        if idx[1] != 'u':
            raise Exception("untagged:browse: called on bad objid %s" % pid)
        idx = int(idx[2:])
    
    if idx >= len(g_utidx):
        raise Exception("untagged:browse: bad pid %s" % pid)

    return idx


# Return entry to be created in the top-level directory ([untagged]).
def rootentries(pid):
    return [rcldirentry(pid + 'untagged', pid, '[untagged]'),]


# Browse method
# objid is like untagged$u<index>
# flag is meta or children.
def browse(pid, flag, httphp, pathprefix):
    idx = _objidtoidx(pid)

    entries = []
    if idx == 0:
        # Browsing root
        for i in range(len(g_utidx))[1:]:
            doc = g_rcldocs[g_utidx[i]]
            id = untg_prefix + '$u' + str(i)
            e = rcldoctoentry(id, pid, httphp, pathprefix, doc)
            if e:
                entries.append(e)
    else:
        # Non root: only items in there. flag needs to be 'meta'
        doc = g_rcldocs[thisdocidx]
        id = untg_prefix + '$u' + str(idx)
        e = rcldoctoentry(id, pid, httphp, pathprefix, doc)
        if e:
            entries.append(e)

    return sorted(entries, key=lambda entry: entry['tt'].lower())
