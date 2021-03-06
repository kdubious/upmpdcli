#
# Copyright (C) 2019 J.F.Dockes
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

# Manage the playlists section of the tree
#
# Object Id prefix: 0$uprcl$playlists
# 
# Obect id inside the section: $p<idx> where <idx> is the document index
#  inside the global document vector.

import os, sys
PY3 = sys.version > '3'

from upmplgutils import uplog
from uprclutils import rcldoctoentry, rcldirentry, cmpentries
import uprclutils
import uprclinit
from recoll import recoll

class Playlists(object):
    def __init__(self, rcldocs, httphp, pathprefix):
        self._idprefix = '0$uprcl$playlists'
        self._httphp = httphp
        self._pprefix = pathprefix
        self.recoll2playlists(rcldocs)
        
    # Create the untagged entries static vector by filtering the global
    # doc vector, storing the indexes of the playlists.
    # We keep a reference to the doc vector.
    def recoll2playlists(self, rcldocs):
        # The -1 entry is because we use index 0 for our root.
        self.utidx = [-1]
    
        for docidx in range(len(rcldocs)):
            doc = rcldocs[docidx]
            if doc.mtype == 'audio/x-mpegurl':
                self.utidx.append(docidx)

    # Compute index into our entries vector by 'parsing' the objid.
    def _objidtoidx(self, pid):
        #uplog("playlists:objidtoidx: %s" % pid)
        if not pid.startswith(self._idprefix):
            raise Exception("playlists:browse: bad pid %s" % pid)

        idx = pid[len(self._idprefix):]
        if not idx:
            # Browsing the root.
            idx = 0
        else:
            if idx[1] != 'p':
                raise Exception("playlists:browse: bad objid %s" % pid)
            idx = int(idx[2:])
    
        if idx >= len(self.utidx):
            raise Exception("playlists:browse: bad pid %s" % pid)

        return idx

    # Return entry to be created in the top-level directory ([playlists]).
    def rootentries(self, pid):
        return [rcldirentry(pid + 'playlists', pid,
                            str(len(self.utidx) - 1) + ' playlists'),]


    # Browse method
    # objid is like playlists$p<index>
    # flag is meta or children.
    def browse(self, pid, flag):
        idx = self._objidtoidx(pid)

        folders = uprclinit.g_trees['folders']
        rcldocs = folders.rcldocs()
        entries = []
        if idx == 0:
            # Browsing root
            for i in range(len(self.utidx))[1:]:
                doc = rcldocs[self.utidx[i]]
                id = self._idprefix + '$p' + str(i)
                title = doc.title if doc.title else doc.filename
                e = rcldirentry(id, pid, title,
                                upnpclass='object.container.playlistContainer')
                if e:
                    entries.append(e)

        else:
            pldoc = rcldocs[self.utidx[idx]]
            plpath = uprclutils.docpath(pldoc)
            #uplog("playlists: plpath %s" % plpath)
            try:
                m3u = uprclutils.M3u(plpath)
            except Exception as ex:
                uplog("M3u open failed: %s %s" % (plpath,ex))
                return entries
            cnt = 1
            for url in m3u:
                doc = recoll.Doc()
                if m3u.urlRE.match(url):
                    # Actual URL (usually http). Create bogus doc
                    doc.setbinurl(bytearray(url))
                    elt = os.path.split(url)[1]
                    doc.title = elt.decode('utf-8', errors='ignore')
                    doc.mtype = "audio/mpeg"
                else:
                    doc.setbinurl(bytearray(b'file://' + url))
                    fathidx, docidx = folders._stat(doc)
                    if docidx < 0:
                        uplog("playlists: can't stat %s"%doc.getbinurl())
                        continue
                    doc = rcldocs[docidx]

                id = pid + '$e' + str(len(entries))
                e = rcldoctoentry(id, pid, self._httphp, self._pprefix, doc)
                if e:
                    entries.append(e)

        return sorted(entries, key=cmpentries)
