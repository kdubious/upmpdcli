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
import sqlite3
from timeit import default_timer as timer

from uprclutils import *

# After initialization, this holds the list of all records out of recoll
g_alldocs = []

g_myprefix = '0$uprcl$'

sqconn = sqlite3.connect(':memory:')
#sqconn = sqlite3.connect('/tmp/tracks.sqlite')

# Tags for which we create auxiliary tables for facet descent.
#
# TBD: The list will come from the config file one day
#
# TBD: All Artists, Group
#
# Maybe we'd actually need a 3rd value for the recoll field name, but
# it can be the same for the currently relevant fields.
tagtables = {
    'Artist' : 'artist',
    'Date' : 'date',
    'Genre' : 'genre',
#   'All Artists' : 'allartists',
    'Composer' : 'composer',
    'Conductor' : 'conductor',
    'Orchestra' : 'orchestra',
    'Group' : 'contentgroup',
    'Comment' : 'comment'
    }

# Translation only used when fetching fields from the recoll
# record. None at the moment
coltorclfield = {
    }


def colid(col):
    return col + '_id'

# Create the db. Each tag table has 2 columns: <tagname>_id and
# value. The join column in the main tracks table is also named
# <tagname>_id
def createsqdb(conn):
    c = conn.cursor()
    try:
        c.execute('''DROP TABLE albums''')
        c.execute('''DROP TABLE tracks''')
    except:
        pass
    c.execute(
        '''CREATE TABLE albums
           (album_id INTEGER PRIMARY KEY, albtitle TEXT, albfolder TEXT)''')

    tracksstmt = '''CREATE TABLE tracks
    (docidx INT, album_id INT, trackno INT, title TEXT'''

    for tb in tagtables.itervalues():
        try:
            c.execute('DROP TABLE ' + tb)
        except:
            pass
        stmt = 'CREATE TABLE ' + tb + \
               ' (' + colid(tb) + ' INTEGER PRIMARY KEY, value TEXT)'
        c.execute(stmt)
        tracksstmt += ',' + colid(tb) + ' INT'

    tracksstmt += ')'
    c.execute(tracksstmt)
    

# Insert new value if not existing, return rowid of new or existing row
def auxtableinsert(sqconn, tb, value):
    c = sqconn.cursor()
    col = colid(tb)
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

# Create the db and fill it up with the values we need, taken out of
# the recoll records list
def recolltosql(docs):
    global g_alldocs
    g_alldocs = docs
    
    createsqdb(sqconn)

    # Compute a list of table names and corresponding recoll
    # fields. most often they are identical
    tabfields = []
    for tb in tagtables.itervalues():
        if tb in coltorclfield:
            rclfld = coltorclfield[tb]
        else:
            rclfld = tb
        tabfields.append((tb, rclfld))
        
    c = sqconn.cursor()
    maxcnt = 0
    totcnt = 0
    for docidx in range(len(docs)):
        doc = docs[docidx]
        totcnt += 1

        # No need to include non-audio types in the visible
        # tree.
        # TBD: We'll have to do some processing on image types though
        # (will go before these lines)
        if doc.mtype not in audiomtypes:
            continue

        album = getattr(doc, 'album', None)
        if not album:
            if doc.mtype != 'inode/directory':
                uplog("No album: mtype %s title %s" % (doc.mtype, doc.url))
            album = '[no album]'
            continue

        folder = docfolder(doc).decode('utf-8', errors = 'replace')

        # tracknos like n/max are now supposedly processed by rclaudio
        # and should not arrive here
        try:
            l= doc.tracknumber.split('/')
            trackno = int(l[0])
        except:
            trackno = 0
            
        # Create album record if needed. There is probably a
        # single-statement syntax for this. The albums table is
        # special, can't use auxtableinsert()
        c.execute('''SELECT album_id FROM albums
        WHERE albtitle = ? AND albfolder = ?''', (album, folder))
        r = c.fetchone()
        if r:
            album_id = r[0]
        else:
            c.execute('''INSERT INTO albums(albtitle, albfolder)
            VALUES (?,?)''', (album, folder))
            album_id = c.lastrowid

        # set base values for column names, values list, placeholders,
        # then append data from auxiliary tables array
        columns = 'docidx,album_id,trackno,title'
        values = [docidx, album_id, trackno, doc.title]
        placehold = '?,?,?,?'
        for tb,rclfld in tabfields:
            value = getattr(doc, rclfld, None)
            if not value:
                continue
            rowid = auxtableinsert(sqconn, tb, value)
            columns += ',' + colid(tb)
            values.append(rowid)
            placehold += ',?'

        stmt = 'INSERT INTO tracks(' + columns + ') VALUES(' + placehold + ')'
        c.execute(stmt, values)
        #uplog(doc.title)

    sqconn.commit()
    uplog("recolltosql: processed %d docs" % totcnt)

# Create our top-level directories, with fixed entries, and stuff from
# the tags tables
def rootentries(pid):
    c = sqconn.cursor()
    c.execute("SELECT COUNT(*) from albums")
    nalbs = str(c.fetchone()[0])
    c.execute("SELECT COUNT(*) from tracks")
    nitems = str(c.fetchone()[0])
    entries = [rcldirentry(pid + 'albums', pid, nalbs + ' albums'),
               rcldirentry(pid + 'items', pid, nitems + ' items')]
    for tt in sorted(tagtables.iterkeys()):
        entries.append(rcldirentry(pid + '=' + tt , pid, tt))
    return entries

# Check what tags still have multiple values inside the selected set,
# and return their list.
def subtreetags(docidsl):
    docids = ','.join([str(i) for i in docidsl])
    uplog("subtreetags, docids %s" % docids)
    c = sqconn.cursor()
    tags = []
    for tt,tb in tagtables.iteritems():
        stmt = 'SELECT COUNT(DISTINCT ' + colid(tb) + \
               ') FROM tracks WHERE docidx IN (' + docids + ')'
        uplog("subtreetags: executing: <%s>" % stmt)
        c.execute(stmt)
        for r in c:
            cnt = r[0]
            uplog("Found %d distinct values for %s" % (cnt, tb))
            if cnt > 1:
                tags.append(tt)
    return tags

def trackentriesforstmt(stmt, values, pid, httphp, pathprefix):
    c = sqconn.cursor()
    c.execute(stmt, values)
    entries = []
    for r in c:
        docidx = r[0]
        id = pid + '$i' + str(docidx)
        entries.append(rcldoctoentry(id, pid, httphp, pathprefix,
                                     g_alldocs[docidx]))
    return sorted(entries, cmp=cmpentries)
    

# Return a list of trackids as selected by the current
# path <selwhere> is like: WHERE col1_id = ? AND col2_id = ? [...], and
# <values> holds the corresponding values
def docidsforsel(selwhere, values):
    c = sqconn.cursor()
    stmt = 'SELECT docidx FROM tracks ' + selwhere + ' ORDER BY trackno'
    uplog("docidsforsel: executing <%s> values %s" % (stmt, values))
    c.execute(stmt, values)
    return [r[0] for r in c.fetchall()]

def trackentriesforalbum(albid, pid, httphp, pathprefix):
    stmt = 'SELECT docidx FROM tracks WHERE album_id = ? ORDER BY trackno'
    return trackentriesforstmt(stmt, (albid,), pid, httphp, pathprefix)
    
# This is called when an 'albums' element is encountered in the
# selection path.
def tagsbrowsealbums(pid, qpath, i, selwhere, values, httphp, pathprefix):
    c = sqconn.cursor()
    docidsl = docidsforsel(selwhere, values)
    entries = []
    if i == len(qpath)-1:
        albidsl = subtreealbums(docidsl)
        albids = ','.join([str(a) for a in albidsl])
        c.execute('SELECT album_id, albtitle FROM albums WHERE album_id in (' +
                  albids + ') ORDER BY albtitle')
        for r in c:
            id = pid + '$' + str(r[0])
            entries.append(rcldirentry(id, pid, r[1]))
    elif i == len(qpath)-2:
        albid = int(qpath[-1])
        docids = ','.join([str(i) for i in docidsl])
        stmt = 'SELECT COUNT(docidx) FROM tracks WHERE album_id = ?'
        c.execute(stmt, (albid,))
        r = c.fetchone()
        ntracks = int(r[0])
        stmt = 'SELECT docidx FROM tracks WHERE album_id = ? AND docidx IN (' +\
               docids + ')'
        entries = trackentriesforstmt(stmt, (albid,), pid, httphp, pathprefix)
        if ntracks != len(entries):
            id = pid + '$' + 'showca'
            entries = [rcldirentry(id, pid, '>> Complete Album')] + entries
    elif i == len(qpath)-3:
        # Note that minim has an additional level here, probably to
        # present groups or multiple groups ? The trackids ids are
        # like: 
        #    0$=Composer$17738$albums$2$showca.0$hcalbum$*i13458
        # I don't know what the .0 is for.
        # The 'hcalbum' level usually has 2 entries '>> Hide Content' 
        # and the album title. TBD
        albid = int(qpath[-2])
        entries = trackentriesforalbum(albid, pid, httphp, pathprefix)
        
    return entries

# This is called when an 'items' element is encountered in the
# selection path. We just list the selected tracks
def tagsbrowseitems(pid, qpath, i, selwhere, values, httphp, pathprefix):
    stmt = 'SELECT docidx FROM tracks ' + selwhere
    return trackentriesforstmt(stmt, values, pid, httphp, pathprefix)


# Return all albums ids to which any of the currently selected tracks
# (designated by a docid set) belong
def subtreealbums(docidsl):
    docids = ','.join([str(r) for r in docidsl])
    albids = []
    stmt = 'SELECT album_id from tracks where docidx IN (' + docids + ') ' + \
           'GROUP BY album_id'
    c = sqconn.cursor()
    uplog('subtreealbums: executing %s' % stmt)
    c.execute(stmt)
    for r in c:
        albids.append(r[0])
    uplog('subtreealbums: returning %s' % albids)
    return albids
    
# Main browsing routine. Given an objid, translate it into a select
# statement, plus further processing, and return the corresponding
# records
def tagsbrowse(pid, qpath, flag, httphp, pathprefix):
    uplog("tagsbrowse. pid %s qpath %s" % (pid, qpath))
    qlen = len(qpath)
    selwhat = ''
    selwhere = ''
    values = []
    i = 0
    while i < qlen:
        elt = qpath[i]

        # '=colname'. Set the current column name, which will be used
        # in different ways depending if this is the last element or
        # not.
        if elt.startswith('='):
            col = tagtables[elt[1:]] 

        
        # detect the special values albums items etc. here. Their
        # presence changes how we process the rest (showing tracks and
        # albums and not dealing with other tags any more)
        if elt == 'albums':
            return tagsbrowsealbums(pid, qpath, i, selwhere, values, httphp,
                                    pathprefix)
        elif elt == 'items':
            return tagsbrowseitems(pid, qpath, i, selwhere, values, httphp,
                                  pathprefix)
            
        selwhere = selwhere + ' AND ' if selwhere else ' WHERE '
        if i == qlen - 1:
            # We want to display all unique values for the column
            # artist.artist_id, artist.value
            selwhat = col + '.' + col + '_id, ' + col + '.value'
            # tracks.artist_id = artist.artist_id
            selwhere += 'tracks.' + colid(col) + ' = ' + col + '.' + colid(col)
        else:
            # Look at the value specified for the =xx column. The
            # selwhat value is only used as a flag
            selwhat = 'tracks.docidx'
            selwhere += 'tracks.' + colid(col) + ' =  ?'
            i += 1
            values.append(int(qpath[i]))
        i += 1
            

    # TBD: Need a ">> Complete Album" entry if there is a single
    # album, no subqs and not all the tracks are listed
    entries = []
    if selwhat == 'tracks.docidx':
        docids = docidsforsel(selwhere, values)
        albids = subtreealbums(docids)
        subqs = subtreetags(docids)
        if len(albids) > 1:
            id = pid + '$albums'
            entries.append(rcldirentry(id, pid, str(len(albids)) + ' albums'))
            if subqs:
                id = pid + '$items'
                entries.append(rcldirentry(id,pid, str(len(docids)) + ' items'))
        elif len(albids) == 1 and subqs:
            id = pid + '$items'
            entries.append(rcldirentry(id, pid, str(len(docids)) + ' items'))

        if not subqs:
            for docidx in docids:
                id = pid + '$*i' + str(docidx)
                entries.append(rcldoctoentry(id, pid, httphp, pathprefix,
                                             g_alldocs[docidx]))
                entries = sorted(entries, cmp=cmpentries)
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
               " GROUP BY tracks." + colid(col) + \
               " ORDER BY value"
        uplog("tagsbrowse: executing <%s> values %s" % (stmt, values))
        c = sqconn.cursor()
        c.execute(stmt, values)
        for r in c:
            id = pid + '$' + str(r[0])
            entries.append(rcldirentry(id, pid, r[1]))
    return entries


# Browse the top-level tree named like 'xxx albums'. There are just 2
# levels: the whole albums list, then for each entry the specified
# albums track list
def albumsbrowse(pid, qpath, flag, httphp, pathprefix):
    c = sqconn.cursor()
    entries = []
    if len(qpath) == 1:
        c.execute('SELECT album_id, albtitle FROM albums ORDER BY albtitle')
        for r in c:
            id = pid + '$*' + str(r[0])
            entries.append(rcldirentry(id, pid, r[1]))
    elif len(qpath) == 2:
        e1 = qpath[1]
        if not e1.startswith("*"):
            raise Exception("Bad album id in albums tree. Pth: %s" %idpath)
        album_id = int(e1[1:])
        entries = trackentriesforalbum(album_id, pid, httphp, pathprefix)
    else:
        raise Exception("Bad path in album tree (too deep): <%s>"%idpath)

    return entries


# Top level browse routine. Handle the special cases and call the
# appropriate worker routine.
def browse(pid, flag, httphp, pathprefix):
    idpath = pid.replace(g_myprefix, '', 1)
    uplog('tags:browse: idpath <%s>' % idpath)
    entries = []
    qpath = idpath.split('$')
    if idpath.startswith('items'):
        stmt = 'SELECT docidx FROM tracks'
        entries = trackentriesforstmt(stmt, (), pid, httphp, pathprefix)
    elif idpath.startswith('albums'):
        entries = albumsbrowse(pid, qpath, flag, httphp, pathprefix)
    elif idpath.startswith('='):
        entries = tagsbrowse(pid, qpath, flag, httphp, pathprefix)
    else:
        raise Exception('Bad path in tags tree (start): <%s>' % idpath)
    return entries









############ Misc test/trial code, not used by uprcl ########################

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
    
