from __future__ import print_function

import os
import shlex
import urllib
import sys

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
#  - i is an index into dirvec if the name is a directory, else 0
#  - j is the index of the doc inside the doc array
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
        dirvec[0][d] = (topidx, 0)
        dirvec.append({})

    for docidx in range(len(docs)):
        doc = docs[docidx]
        url = doc.getbinurl()
        url = url[7:]
        try:
            decoded = url.decode('utf-8')
        except:
            decoded = urllib.quote(url).decode('utf-8')

        fathidx = -1
        for rtpath,idx in dirvec[0].iteritems():
            if url.startswith(rtpath):
                fathidx = idx[0]
                break
        if fathidx == -1:
            print("No parent in topdirs: %s" % decoded)
            continue

        url1 = url[len(rtpath):]
        if len(url1) == 0:
            continue

        path = url1.split('/')[1:]
        #print("%s"%path, file=sys.stderr)
        for idx in range(len(path)):
            elt = path[idx]
            if elt in dirvec[fathidx]:
                fathidx = dirvec[fathidx][elt][0]
            else:
                if idx != len(path) -1 or doc.mtype == 'inode/directory':
                    topidx += 1
                    dirvec.append({})
                    dirvec[fathidx][elt] = (topidx, docidx)
                    fathidx = topidx
                else:
                    dirvec[fathidx][elt] = (topidx, docidx)

    if False:
        for ent in dirvec:
            print("%s" % ent)



def fetchalldocs(confdir):
    global allthedocs
    allthedocs = []

    rcldb = recoll.connect(confdir=confdir)
    rclq = rcldb.query()
    rclq.execute("mime:*", stemming=0)
    print("Estimated query results: %d" % (rclq.rowcount))

    maxcnt = 0
    totcnt = 0
    while True:
        docs = rclq.fetchmany()
        for doc in docs:
            allthedocs.append(doc)
            totcnt += 1
        if (maxcnt > 0 and totcnt >= maxcnt) or len(docs) != rclq.arraysize:
            break
    print("Retrieved %d docs" % (totcnt,))

fetchalldocs(confdir)
rcl2folders(allthedocs)

print("%s" % dirvec[0])
print("%s" % dirvec[1])
print("%s" % dirvec[2])
print("%s" % dirvec[3])
