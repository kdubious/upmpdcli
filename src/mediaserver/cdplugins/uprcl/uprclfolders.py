
import os
import shlex
import urllib
import sys

from uprclutils import *

from recoll import recoll
from recoll import rclconfig

g_myprefix = '0$uprcl$folders'


# Internal init: create the directory tree (folders view) from the doc
# array by splitting the url in each doc.
#
# The dirvec vector has one entry for each directory. Each entry is a
# dictionary, mapping the names inside the directory to a pair (i,j),
# where:
#  - i is an index into dirvec if the name is a directory, else -1
#  - j is the index of the doc inside the doc array (or -1 if there is no doc)
#
# Entry 0 in dirvec is special: it holds the 'topdirs' from the recoll
# configuration. The entries are paths instead of simple names, and
# the doc index (j) is 0. The dir index points normally to a dirvec
# entry.

# Create new directory entry: insert in father and append dirvec slot (with ".." entry)
def _createdir(dirvec, fathidx, docidx, nm):
    dirvec.append({})
    dirvec[fathidx][nm] = (len(dirvec) - 1, docidx)
    dirvec[-1][".."] = (fathidx, -1)
    return len(dirvec) - 1

def _rcl2folders(docs, confdir):
    global dirvec
    dirvec = []

    rclconf = rclconfig.RclConfig(confdir)
    topdirs = [os.path.expanduser(d) for d in
               shlex.split(rclconf.getConfParam('topdirs'))]
    topdirs = [d.rstrip('/') for d in topdirs]

    dirvec.append({})
    dirvec[0][".."] = (0, -1)
    for d in topdirs:
        dirvec.append({})
        dirvec[0][d] = (len(dirvec)-1, -1)
        dirvec[-1][".."] = (0, -1)

    # Walk the doc list and update the directory tree according to the
    # url (create intermediary directories if needed, create leaf
    # entry
    for docidx in range(len(docs)):
        doc = docs[docidx]
        url = doc.getbinurl()
        url = url[7:]
        try:
            decoded = url.decode('utf-8')
        except:
            decoded = urllib.quote(url).decode('utf-8')

        # Determine the root entry (topdirs element). Special because
        # path not simple name
        fathidx = -1
        for rtpath,idx in dirvec[0].iteritems():
            if url.startswith(rtpath):
                fathidx = idx[0]
                break
        if fathidx == -1:
            uplog("No parent in topdirs: %s" % decoded)
            continue

        # Compute rest of path
        url1 = url[len(rtpath):]
        if len(url1) == 0:
            continue

        # Split path, then walk the vector, possibly creating
        # directory entries as needed
        path = url1.split('/')[1:]
        #uplog("%s"%path, file=sys.stderr)
        for idx in range(len(path)):
            elt = path[idx]
            if elt in dirvec[fathidx]:
                # This path element was already seen
                # If this is the last entry in the path, maybe update
                # the doc idx (previous entries were created for
                # intermediate elements without a Doc).
                #uplog("NEED TO UPDATE DOC")
                dirvec[fathidx][elt] = (dirvec[fathidx][elt][0], docidx)
                # Update fathidx for next iteration
                fathidx = dirvec[fathidx][elt][0]
            else:
                # Element has no entry in father directory (hence no
                # dirvec entry either).
                if idx != len(path) -1:
                    # This is an intermediate element. Create a
                    # Doc-less directory
                    fathidx = _createdir(dirvec, fathidx, -1, elt)
                else:
                    # Last element. If directory, needs a dirvec entry
                    if doc.mtype == 'inode/directory':
                        fathidx = _createdir(dirvec, fathidx, docidx, elt)
                    else:
                        dirvec[fathidx][elt] = (-1, docidx)

    if False:
        for ent in dirvec:
            uplog("%s" % ent)

    return dirvec

# Internal init: fetch all the docs by querying Recoll with [mime:*],
# which is guaranteed to match every doc without overflowing the query
# size (because the number of mime types is limited). Something like
# title:* would overflow.
def _fetchalldocs(confdir):
    allthedocs = []

    rcldb = recoll.connect(confdir=confdir)
    rclq = rcldb.query()
    rclq.execute("mime:*", stemming=0)
    uplog("Estimated alldocs query results: %d" % (rclq.rowcount))

    maxcnt = 2000
    totcnt = 0
    while True:
        docs = rclq.fetchmany()
        for doc in docs:
            allthedocs.append(doc)
            totcnt += 1
        if (maxcnt > 0 and totcnt >= maxcnt) or len(docs) != rclq.arraysize:
            break
    uplog("Retrieved %d docs" % (totcnt,))
    return allthedocs


# Initialize (read recoll data and build tree)
def inittree(confdir):
    global g_alldocs, g_dirvec
    
    g_alldocs = _fetchalldocs(confdir)
    g_dirvec = _rcl2folders(g_alldocs, confdir)
    return g_alldocs

def _objidtodiridx(pid):
    if not pid.startswith(g_myprefix):
        raise Exception("folders.browse: bad pid %s" % pid)

    if len(g_alldocs) == 0:
        raise Exception("folders:browse: no docs")

    diridx = pid[len(g_myprefix):]
    if not diridx:
        diridx = 0
    else:
        if diridx[1] != 'd':
            raise Exception("folders:browse: called on non dir objid %s" % pid)
        diridx = int(diridx[2:])
    
    if diridx >= len(g_dirvec):
        raise Exception("folders:browse: bad pid %s" % pid)

    return diridx

def rootentries(pid):
    return [rcldirentry(pid + 'folders', pid, '[folders]'),]

# Browse method
# objid is like folders$index
# flag is meta or children. 
def browse(pid, flag, httphp, pathprefix):

    diridx = _objidtodiridx(pid)

    entries = []

    # The basename call is just for diridx==0 (topdirs). Remove it if
    # this proves a performance issue
    for nm,ids in g_dirvec[diridx].iteritems():
        #uplog("folders:browse: got nm %s" % nm.decode('utf-8'))
        if nm == "..":
            continue
        thisdiridx = ids[0]
        thisdocidx = ids[1]
        if thisdiridx >= 0:
            id = g_myprefix + '$' + 'd' + str(thisdiridx)
            entries.append(rcldirentry(id, pid, os.path.basename(nm)))
        else:
            # Not a directory. docidx had better been set
            if thisdocidx == -1:
                uplog("folders:docidx -1 for non-dir entry %s"%nm)
                continue
            doc = g_alldocs[thisdocidx]
            id = g_myprefix + '$i' + str(thisdocidx)
            e = rcldoctoentry(id, pid, httphp, pathprefix, doc)
            if e:
                entries.append(e)

    return sorted(entries, cmp=cmpentries)

# return path for objid, which has to be a container. This is good old pwd
def dirpath(objid):

    # We may get called from search, on the top dir (above [folders]). Return
    # empty in this case
    try:
        diridx = _objidtodiridx(objid)
    except:
        return ""

    if diridx == 0:
        return "/"
    
    lpath = []
    while True:
        fathidx = g_dirvec[diridx][".."][0]
        for nm, ids in g_dirvec[fathidx].iteritems():
            if ids[0] == diridx:
                lpath.append(nm)
                break
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
