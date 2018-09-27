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

# Manage the [folders] section of the tree.
#
# Object Id prefix: 0$uprcl$folders
# 
# Obect id inside the section.
#    Container: $d<diridx> where <diridx> indexes into our directory vector.
#    Item: $i<docidx> where <docidx> indexex into the docs vector.
#
# Data structure:
#
# The _dirvec vector has one entry for each directory. Each entry is a
# dictionary, mapping the names inside the directory to a pair
# (diridx,docidx), where:
#  - diridx is an index into dirvec if the name is a directory, else -1
#  - docidx is the index of the doc inside the doc array, or -1 if:
#     - There is no doc entry, which could possibly happen if there is
#       no result for an intermediary element in a path,
#       because of some recoll issue, or because this is a synthetic
#       'contentgroup' entry.
#     - Or if the doc was not yet seen, the index will then be updated
#       when we see it.
#
# Each directory has a special ".." entry with a diridx pointing to
# the parent directory. This allows building a path from a container
# id (aka pwd).
#
# No need has emerged for a "." entry.
# 
# Entry 0 in _dirvec is special: it holds the 'topdirs' from the recoll
# configuration. The entries are paths instead of simple names, and
# the docidx is 0. The diridx points to a dirvec entry.

import os
import shlex
import sys
PY3 = sys.version > '3'
if PY3:
    from urllib.parse import quote as urlquote
else:
    from urllib import quote as urlquote

import time
from timeit import default_timer as timer

from upmplgutils import uplog
from uprclutils import docarturi, audiomtypes, rcldirentry, \
     rcldoctoentry, cmpentries
from recoll import recoll
from recoll import rclconfig

class Folders(object):

    # Initialize (read recoll data and build tree).
    def __init__(self, confdir, httphp, pathprefix):
        self._idprefix = '0$uprcl$folders'
        self._httphp = httphp
        self._pprefix = pathprefix
        # Debug : limit processed recoll entries for speed
        self._maxrclcnt = 0
        self._fetchalldocs(confdir)
        self._rcl2folders(confdir)

    def rcldocs(self):
        return self._rcldocs
    
    # Create new directory entry: insert in father and append dirvec slot
    # (with ".." entry)
    def _createdir(self, fathidx, docidx, nm):
        self._dirvec.append({})
        self._dirvec[fathidx][nm] = (len(self._dirvec) - 1, docidx)
        self._dirvec[-1][".."] = (fathidx, -1)
        return len(self._dirvec) - 1


    # Walk the recoll docs array and split the URLs paths to build the
    # [folders] data structure
    def _rcl2folders(self, confdir):
        self._dirvec = []
        start = timer()

        rclconf = rclconfig.RclConfig(confdir)
        topdirs = [os.path.expanduser(d) for d in
                   shlex.split(rclconf.getConfParam('topdirs'))]
        topdirs = [d.rstrip('/') for d in topdirs]

        # Create the 1st entry. This is special because it holds the
        # recoll topdirs, which are paths instead of simple names. There
        # does not seem any need to build the tree between a topdir and /
        self._dirvec.append({})
        self._dirvec[0][".."] = (0, -1)
        for d in topdirs:
            self._dirvec.append({})
            self._dirvec[0][d] = (len(self._dirvec)-1, -1)
            self._dirvec[-1][".."] = (0, -1)

        # Walk the doc list and update the directory tree according to the
        # url: create intermediary directories if needed, create leaf
        # entry.
        #
        # Binary path issue: at the moment the python rclconfig can't
        # handle binary (the underlying conftree.py can, we'd need a
        # binary stringToStrings). So the topdirs entries have to be
        # strings, and so we decode the binurl too. This probably
        # could be changed we wanted to support binary, (non utf-8)
        # paths. For now, for python3 all dir/file names in the tree
        # are str
        for docidx in range(len(self._rcldocs)):
            doc = self._rcldocs[docidx]
            
            # No need to include non-audio types in the visible tree.
            if doc.mtype not in audiomtypes:
                continue

            # Possibly enrich the doc entry with a cover art uri.
            arturi = docarturi(doc, self._httphp, self._pprefix)
            if arturi:
                # The uri is quoted, so it's ascii and we can just store
                # it as a doc attribute
                doc.albumarturi = arturi

            url = doc.getbinurl().decode('utf-8', errors='replace')
            url = url[7:]
            # Determine the root entry (topdirs element). Special because
            # its path is not a simple name.
            fathidx = -1
            for rtpath,idx in self._dirvec[0].items():
                #uplog("type(url) %s type(rtpath) %s rtpath %s url %s" %
                # (type(url),type(rtpath),rtpath, url))
                if url.startswith(rtpath):
                    fathidx = idx[0]
                    break
            if fathidx == -1:
                uplog("No parent in topdirs: %s" % url)
                continue

            # Compute rest of path
            url1 = url[len(rtpath):]
            if len(url1) == 0:
                continue

            # If there is a contentgroup field, just add it as a virtual
            # directory in the path. This only affects the visible tree,
            # not the 'real' URLs of course.
            if doc.contentgroup:
                a = os.path.dirname(url1)
                b = os.path.basename(url1)
                url1 = os.path.join(a, doc.contentgroup, b)
            
            # Split path, then walk the vector, possibly creating
            # directory entries as needed
            path = url1.split('/')[1:]
            #uplog("%s"%path, file=sys.stderr)
            for idx in range(len(path)):
                elt = path[idx]
                if elt in self._dirvec[fathidx]:
                    # This path element was already seen
                    # If this is the last entry in the path, maybe update
                    # the doc idx (previous entries were created for
                    # intermediate elements without a Doc).
                    if idx == len(path) -1:
                        self._dirvec[fathidx][elt] = \
                                   (self._dirvec[fathidx][elt][0], docidx)
                    # Update fathidx for next iteration
                    fathidx = self._dirvec[fathidx][elt][0]
                else:
                    # Element has no entry in father directory (hence no
                    # self._dirvec entry either).
                    if idx != len(path) -1:
                        # This is an intermediate element. Create a
                        # Doc-less directory
                        fathidx = self._createdir(fathidx, -1, elt)
                    else:
                        # Last element. If directory, needs a self._dirvec entry
                        if doc.mtype == 'inode/directory':
                            fathidx = self._createdir(fathidx, docidx, elt)
                        else:
                            self._dirvec[fathidx][elt] = (-1, docidx)

        if False:
            for ent in self._dirvec:
                uplog("%s" % ent)

        end = timer()
        uplog("_rcl2folders took %.2f Seconds" % (end - start))

    # Fetch all the docs by querying Recoll with [mime:*], which is
    # guaranteed to match every doc without overflowing the query size
    # (because the number of mime types is limited). Something like
    # title:* would overflow. This creates the main doc array, which is
    # then used by all modules.
    def _fetchalldocs(self, confdir):
        start = timer()

        rcldb = recoll.connect(confdir=confdir)
        rclq = rcldb.query()
        rclq.execute("mime:*", stemming=0)
        uplog("Estimated alldocs query results: %d" % (rclq.rowcount))

        totcnt = 0
        self._rcldocs = []
        while True:
            # There are issues at the end of list with fetchmany (sets
            # an exception). Works in python2 for some reason, but
            # breaks p3. Until recoll is fixed, catch exception
            # here. Also does not work if we try to fetch by bigger
            # slices (we get an exception and a truncated list)
            try:
                docs = rclq.fetchmany()
                for doc in docs:
                    self._rcldocs.append(doc)
                    totcnt += 1
            except:
                docs = []
            if (self._maxrclcnt > 0 and totcnt >= self._maxrclcnt) or \
                   len(docs) != rclq.arraysize:
                break
            time.sleep(0)
        end = timer()
        uplog("Retrieved %d docs in %.2f Seconds" % (totcnt,end - start))


    ##############
    # Browsing the initialized [folders] hierarchy

    # Extract dirvec index from objid, according to the way we generate them.
    def _objidtodiridx(self, pid):
        if not pid.startswith(self._idprefix):
            raise Exception("folders.browse: bad pid %s" % pid)

        if len(self._rcldocs) == 0:
            raise Exception("folders:browse: no docs")

        diridx = pid[len(self._idprefix):]
        if not diridx:
            diridx = 0
        else:
            if diridx[1] != 'd':
                raise Exception("folders:browse: called on non dir objid %s" %
                                pid)
            diridx = int(diridx[2:])
            
        if diridx >= len(self._dirvec):
            raise Exception("folders:browse: bad pid %s" % pid)

        return diridx


    # Tell the top module what entries we define in the root
    def rootentries(self, pid):
        return [rcldirentry(pid + 'folders', pid, '[folders]'),]


    # Look all non-directory docs inside directory, and return the cover
    # art we find.
    def _arturifordir(self, diridx):
        for nm,ids in self._dirvec[diridx].items():
            if ids[1] >= 0:
                doc = self._rcldocs[ids[1]]
                if doc.mtype != 'inode/directory' and doc.albumarturi:
                    return doc.albumarturi
              

    # Folder hierarchy browse method.
    # objid is like folders$index
    # flag is meta or children.
    def browse(self, pid, flag):

        diridx = self._objidtodiridx(pid)

        # If there is only one entry in root, skip it. This means that 0
        # and 1 point to the same dir, but this does not seem to be an
        # issue
        if diridx == 0 and len(self._dirvec[0]) == 2:
            diridx = 1
        
        entries = []

        # The basename call is just for diridx==0 (topdirs). Remove it if
        # this proves a performance issue
        for nm,ids in self._dirvec[diridx].items():
            if nm == "..":
                continue
            thisdiridx = ids[0]
            thisdocidx = ids[1]
            if thisdocidx >= 0:
                doc = self._rcldocs[thisdocidx]
            else:
                uplog("No doc for %s" % pid)
                doc = None
            
            if thisdiridx >= 0:
                # Skip empty directories
                if len(self._dirvec[thisdiridx]) == 1:
                    continue
                id = self._idprefix + '$' + 'd' + str(thisdiridx)
                if doc and doc.albumarturi:
                    arturi = doc.albumarturi
                else:
                    arturi = self._arturifordir(thisdiridx)
                entries.append(rcldirentry(id, pid, os.path.basename(nm),
                                           arturi=arturi))
            else:
                # Not a directory. docidx had better been set
                if thisdocidx == -1:
                    uplog("folders:docidx -1 for non-dir entry %s"%nm)
                    continue
                doc = self._rcldocs[thisdocidx]
                id = self._idprefix + '$i' + str(thisdocidx)
                e = rcldoctoentry(id, pid, self._httphp, self._pprefix, doc)
                if e:
                    entries.append(e)

        if PY3:
            return sorted(entries, key=cmpentries)
        else:
            return sorted(entries, cmp=cmpentries)

    # Return path for objid, which has to be a container.This is good old
    # pwd... It is called from the search module for generating a 'dir:'
    # recoll filtering directive.
    def dirpath(self, objid):
        # We may get called from search, on the top dir (above
        # [folders]). Return empty in this case
        try:
            diridx = self._objidtodiridx(objid)
        except:
            return ""

        if diridx == 0:
            return "/"
    
        lpath = []
        while True:
            fathidx = self._dirvec[diridx][".."][0]
            found = False
            for nm, ids in self._dirvec[fathidx].items():
                if ids[0] == diridx:
                    lpath.append(nm)
                    found = True
                    break
            # End for
            if not found:
                uplog("uprclfolders: pwd failed for %s \
                (father not found), returning /" % objid)
                return "/"
            if len(lpath) > 200:
                uplog("uprclfolders: pwd failed for %s \
                (looping), returning /" % objid)
                return "/"
                
            diridx = fathidx
            if diridx == 0:
                break

        if not lpath:
            path = "/"
        else:
            path = ""
        for elt in reversed(lpath):
            path += elt + "/"

        return path
