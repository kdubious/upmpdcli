
import os
import shlex
import urllib
import sys

from uprclutils import *

from recoll import recoll
from recoll import rclconfig

confdir = "/home/dockes/.recoll-mp3"

rclconf = rclconfig.RclConfig(confdir)

topdirs = [os.path.expanduser(d) for d in
           shlex.split(rclconf.getConfParam('topdirs'))]

# Create the directory tree (folders view) from the doc array by
# splitting the url in each doc.
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
def rcl2folders(docs):
    global dirvec
    dirvec = []

    topidx = 0
    dirvec.append({})
    for d in topdirs:
        topidx += 1
        dirvec[0][d] = (topidx, -1)
        dirvec.append({})

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
                    topidx += 1
                    dirvec.append({})
                    dirvec[fathidx][elt] = (topidx, -1)
                    fathidx = topidx
                else:
                    # Last element. If directory, needs a dirvec entry
                    if doc.mtype == 'inode/directory':
                        topidx += 1
                        dirvec.append({})
                        dirvec[fathidx][elt] = (topidx, docidx)
                        fathidx = topidx
                    else:
                        dirvec[fathidx][elt] = (-1, docidx)

    if False:
        for ent in dirvec:
            uplog("%s" % ent)

    return dirvec

# Fetch all the docs by querying Recoll with [mime:*], which is
# guaranteed to match every doc without overflowing the query size
# (because the number of mime types is limited). Something like
# title:* would overflow.
def fetchalldocs(confdir):
    allthedocs = []

    rcldb = recoll.connect(confdir=confdir)
    rclq = rcldb.query()
    rclq.execute("mime:*", stemming=0)
    uplog("Estimated alldocs query results: %d" % (rclq.rowcount))

    maxcnt = 0
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

def inittree(confdir):
    global g_alldocs, g_dirvec
    
    g_alldocs = fetchalldocs(confdir)
    g_dirvec = rcl2folders(g_alldocs)


g_myprefix = '0$uprcl$folders'

# objid is like folders$index
# flag is meta or children. 
def browse(pid, flag):
    global g_alldocs, g_dirvec

    if not pid.startswith(g_myprefix):
        uplog("folders.browse: bad pid %s" % pid)
        return []

    try:
        len(g_alldocs)
    except:
        inittree(confdir)

    if len(g_alldocs) == 0:
        uplog("folders:browse: no docs")
        return []

    diridx = pid[len(g_myprefix):]
    if not diridx:
        diridx = 0
    else:
        diridx = int(diridx[1:])
    
    if diridx >= len(g_dirvec):
        uplog("folders:browse: bad pid %s" % pid)
        return []

    entries = []
    Need to treat diridx 0 special (take simple paths)
    for nm,ids in g_dirvec[diridx].iteritems():
        id = g_myprefix + '$' + str(ids[0])
        entries.append(rcldirentry(id, pid, nm))

    return entries
