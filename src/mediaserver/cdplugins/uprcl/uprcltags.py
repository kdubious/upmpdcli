from __future__ import print_function

import sys
import sqlite3
from timeit import default_timer as timer

from uprclutils import *

sqconn = sqlite3.connect(':memory:')
#sqconn = sqlite3.connect('/tmp/tracks.sqlite')

# TBD All Artists, Orchestra, Group
#
# Maybe we'd actually need a 3rd value for the recoll field name, but
# it can be the same for the currently relevant fields.
tables = {'Artist' : 'artist',
          'Date' : 'date',
          'Genre' : 'genre',
          'All Artists' : 'allartists',
          'Composer' : 'composer',
          'Conductor' : 'conductor',
          'Orchestra' : 'orchestra',
          'Group' : 'grouptb',
          'Comment' : 'comment'
          }
          
def createsqdb(conn):
    c = conn.cursor()
    try:
        c.execute('''DROP TABLE albums''')
        c.execute('''DROP TABLE tracks''')
    except:
        pass
    c.execute(
        '''CREATE TABLE albums
           (albid INTEGER PRIMARY KEY, albtitle TEXT, albfolder TEXT)''')

    tracksstmt = '''CREATE TABLE tracks
    (docidx INT, albid INT, trackno INT, title TEXT'''

    for tt,tb in tables.iteritems():
        try:
            c.execute('DROP TABLE ' + tb)
        except:
            pass
        stmt = 'CREATE TABLE ' + tb + \
               ' (' + tb + '_id' + ' INTEGER PRIMARY KEY, value TEXT)'
        c.execute(stmt)
        tracksstmt += ',' + tb + '_id INT'

    tracksstmt += ')'
    c.execute(tracksstmt)
    

def auxtableinsert(sqconn, tb, value):
    c = sqconn.cursor()
    col = tb + '_id'
    stmt = 'SELECT ' + col + ' FROM ' + tb + ' WHERE value = ?'
    c.execute(stmt, (value,))
    r = c.fetchone()
    if r:
        rowid = r[0]
    else:
        stmt = 'INSERT INTO ' + tb + '(value) VALUES(?)'
        c.execute(stmt, (value,))
        rowid = c.lastrowid

    return rowid
            
g_alldocs = []
def recolltosql(docs):
    global g_alldocs
    g_alldocs = docs
    
    createsqdb(sqconn)

    c = sqconn.cursor()
    maxcnt = 0
    totcnt = 0
    for docidx in range(len(docs)):
        doc = docs[docidx]
        totcnt += 1
        album = getattr(doc, 'album', None)
        if not album:
            if doc.mtype != 'inode/directory' and \
                   doc.mtype != 'image/jpeg':
                pass
                #uplog("No album: mtype %s title %s" % (doc.mtype, doc.url))
            continue
        folder = docfolder(doc).decode('utf-8', errors = 'replace')
        try:
            l= doc.tracknumber.split('/')
            trackno = int(l[0])
        except:
            trackno = 0
            
        # Create album record if needed. There is probably a
        # single-statement syntax for this
        c.execute('''SELECT albid FROM albums
        WHERE albtitle = ? AND albfolder = ?''', (album, folder))
        r = c.fetchone()
        if r:
            albid = r[0]
        else:
            c.execute('''INSERT INTO albums(albtitle, albfolder)
            VALUES (?,?)''', (album, folder))
            albid = c.lastrowid


        columns = 'docidx,albid,trackno,title'
        values = [docidx, albid, trackno, doc.title]
        placehold = '?,?,?,?'
        for tt,tb in tables.iteritems():
            value = getattr(doc, tb, None)
            if not value:
                continue
            rowid = auxtableinsert(sqconn, tb, value)
            columns += ',' + tb + '_id'
            values.append(rowid)
            placehold += ',?'

        stmt = 'INSERT INTO tracks(' + columns + ') VALUES(' + placehold + ')'
        c.execute(stmt, values)
        #uplog(doc.title)


    sqconn.commit()
    uplog("recolltosql: processed %d docs" % totcnt)

def rootentries(pid):
    entries = [rcldirentry(pid + 'albums', pid, 'albums'),
               rcldirentry(pid + 'items', pid, 'items')]
    for tt,tb in tables.iteritems():
        entries.append(rcldirentry(pid + '=' + tt , pid, tt))
    return entries

g_myprefix = '0$uprcl$'

def colid(col):
    return col + '_id'

def analyzesubtree(sqconn, recs):
    docids = ','.join([str(r[0]) for r in recs])
    uplog("analyzesubtree, docids %s" % docids)
    c1 = sqconn.cursor()
    tags = []
    for tt,tb in tables.iteritems():
        stmt = 'SELECT COUNT(DISTINCT ' + colid(tb) + \
               ') FROM tracks WHERE docidx IN (' + docids + ')'
        uplog("analyzesubtree: executing: <%s>" % stmt)
        c1.execute(stmt)
        for r in c1:
            cnt = r[0]
            uplog("Found %d distinct values for %s" % (cnt, tb))
            if cnt > 1:
                tags.append(tt)
    return tags

def seltagsbrowse(pid, qpath, flag, httphp, pathprefix):
    uplog("seltagsbrowse. qpath %s" % qpath)
    qlen = len(qpath)
    selwhat = ''
    selwhere = ''
    values = []
    i = 0
    while i < qlen:
        elt = qpath[i]
        if elt.startswith('='):
            col = tables[elt[1:]] 
        selwhere = selwhere + ' AND ' if selwhere else ' WHERE '
        if i == qlen - 1:
            # We want to display all unique values for the column
            # artist.artist_id, artist.value
            selwhat = col + '.' + col + '_id, ' + col + '.value'
            # tracks.artist_id = artist.artist_id
            selwhere += 'tracks.'+ col + '_id = ' + col + '.' + col + '_id'
        else:
            # Look at the value specified for the =xx column
            selwhat = 'tracks.docidx'
            selwhere += 'tracks.' + col + '_id =  ?'
            i += 1
            values.append(int(qpath[i]))
        i += 1
            
    c = sqconn.cursor()
    #for r in c:
    #    uplog("selres: %s" % r)
    entries = []
    if selwhat == 'tracks.docidx':
        # SELECT docidx FROM tracks
        # WHERE col1_id = ? AND col2_id = ?
        stmt = "SELECT docidx FROM tracks %s" % selwhere
        uplog("seltagsbrowse: executing <%s> values %s" % (stmt, values))
        c.execute(stmt, values)
        recs = c.fetchall()
        subqs = analyzesubtree(sqconn, recs)
        if not subqs:
            for r in recs:
                docidx = r[0]
                id = pid + '$*i' + str(docidx)
                entries.append(rcldoctoentry(id, pid, httphp, pathprefix,
                                             g_alldocs[docidx]))
        else:
            for tt in subqs:
                id = pid + '$=' + tt
                entries.append(rcldirentry(id, pid, tt))
    else:
        # SELECT col.value FROM tracks, col
        # WHERE tracks.col_id = col.col_id
        # GROUP BY tracks.col_id
        # ORDER BY col.value
        stmt = "SELECT " + selwhat + " FROM tracks, " + col + \
               selwhere + \
               " GROUP BY tracks." + col + '_id' + \
               " ORDER BY value"
        uplog("seltagsbrowse: executing <%s> values %s" % (stmt, values))
        c.execute(stmt, values)
        for r in c:
            id = pid + '$' + str(r[0])
            entries.append(rcldirentry(id, pid, r[1]))
    return entries

def browse(pid, flag, httphp, pathprefix):
    idpath = pid.replace(g_myprefix, '', 1)
    entries = []
    uplog('tags:browse: idpath <%s>' % idpath)
    qpath = idpath.split('$')
    c = sqconn.cursor()
    if idpath.startswith('items'):
        c.execute('''SELECT docidx FROM tracks ORDER BY title''')
        for r in c:
            docidx = r[0]
            id = pid + '$*i' + str(docidx)
            entries.append(rcldoctoentry(id, pid, httphp, pathprefix,
                                         g_alldocs[docidx]))
    elif idpath.startswith('albums'):
        if len(qpath) == 1:
            c.execute('''SELECT albid, albtitle FROM albums
            ORDER BY albtitle''')
            for r in c:
                id = pid + '$*' + str(r[0])
                entries.append(rcldirentry(id, pid, r[1]))
        elif len(qpath) == 2:
            e1 = qpath[1]
            if not e1.startswith("*"):
                raise Exception("Bad album id in albums tree. Pth: %s" %idpath)
            albid = int(e1[1:])
            c.execute('''SELECT docidx FROM tracks WHERE albid = ? ORDER BY
            trackno''', (albid,))
            for r in c:
                docidx = r[0]
                id = pid + '$*i' + str(docidx)
                entries.append(rcldoctoentry(id, pid, httphp, pathprefix,
                                             g_alldocs[docidx]))
        else:
            raise Exception("Bad path in album tree (too deep): <%s>"%idpath)
    elif idpath.startswith('='):
        entries = seltagsbrowse(pid, qpath, flag, httphp, pathprefix)
    else:
        raise Exception('Bad path in tags tree (start>):<%s>'%idpath)
    return entries


def misctries():
    c = sqconn.cursor()
    c.execute('''SELECT COUNT(*) FROM tracks''')
    uplog("Count(*) %d" % (c.fetchone()[0],))
    
    #for row in c.execute('''SELECT album
    #                        FROM tracks where artist LIKE "%Gould%"
    #                        GROUP BY album'''):
    #    uplog("%s" % (row,))

    # For some strange reason it appears that GROUP BY is faster than SELECT
    # DISTINCT
    stmt = '''SELECT album FROM tracks GROUP BY album ORDER BY album'''
    start = timer()
    for row in c.execute(stmt):
        #uplog("%s" % (row[0].encode('UTF-8')))
        pass
    end = timer()
    uplog("Select took %.2f Seconds" % (end - start))
    for row in c.execute('''SELECT COUNT(DISTINCT album) from tracks'''):
        uplog("Album count %d" % row[0])


if __name__ == '__main__':
    confdir = "/home/dockes/.recoll-mp3"
    from recoll import recoll

    def fetchalldocs(confdir):
        allthedocs = []
        rcldb = recoll.connect(confdir=confdir)
        rclq = rcldb.query()
        rclq.execute("mime:*", stemming=0)
        uplog("Estimated alldocs query results: %d" % (rclq.rowcount))
        maxcnt = 1000
        totcnt = 0
        while True:
            docs = rclq.fetchmany()
            for doc in docs:
                allthedocs.append(doc)
                totcnt += 1
            if (maxcnt > 0 and totcnt >= maxcnt) or \
                   len(docs) != rclq.arraysize:
                break
        uplog("Retrieved %d docs" % (totcnt,))
        return allthedocs
    
    start = timer()
    docs = fetchalldocs(confdir)
    end = timer()
    uplog("Recoll extract took %.2f Seconds" % (end - start))
    start = timer()
    recolltosql(docs)
    end = timer()
    uplog("SQL db create took %.2f Seconds" % (end - start))
    
