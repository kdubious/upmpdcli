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
PY3 = sys.version > '3'
import os
import sqlite3
import time
import tempfile
from timeit import default_timer as timer

from upmplgutils import uplog
from uprclutils import g_myprefix, audiomtypes, docfolder, \
     rcldirentry, rcldoctoentry, cmpentries

# Tags for which we create auxiliary tables for facet descent.
#
# TBD: The list will come from the config file one day
# TBD: alias et al configuration
#
# TBD: All Artists
#
# The key is the presentation name (container title). The value is the
# auxiliary table name, used also as base for unique id and join
# columns (with _id) appended, and is also currently the recoll field
# name (with a provision to differ if needed, thanks to the
# currently empty _coltorclfield dict).
_tagtotable = {
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

def _clid(table):
    return table + '_id'

# Translation only used when fetching fields from the recoll
# record. None at the moment
_coltorclfield = {
    }



# The browseable object which defines the tree of tracks organized by tags.
class Tagged(object):
    def __init__(self, docs, httphp, pathprefix):
        self._httphp = httphp
        self._pprefix = pathprefix
        self._conn = None
        self._rcldocs = docs
        self._init_sqconn()
        # Compute an array of (table name, recoll field) translations. Most
        # often they are identical.
        self._tabtorclfield = []
        for tb in _tagtotable.values():
            if tb in _coltorclfield:
                rclfld = _coltorclfield[tb]
            else:
                rclfld = tb
            self._tabtorclfield.append((tb, rclfld))
        self._recolltosql(docs)
        

    def _init_sqconn(self):
        # We use a separate thread for building the db to ensure
        # responsiveness during this phase.  :memory: handles normally
        # can't be shared between threads, and different :memory: handles
        # access different dbs. The following would work, but it needs
        # python 3.4
        #self._conn = sqlite3.connect('file:uprcl_db?mode=memory&cache=shared')
        # As we can guarantee that 2 threads will never access the db at
        # the same time (the init thread just goes away when it's done),
        # we just disable the same_thread checking on :memory:
        if self._conn is None:
            self._conn = sqlite3.connect(':memory:', check_same_thread=False)


    # Create an empty db.
    #
    # There is then one table for each tag (Artist, Genre, Date,
    # etc.).  Each tag table has 2 columns: <tagname>_id and value.
    # 
    # The tracks table is the "main" table, and has a record for each
    # track, with a title column, and one join column for each tag, 
    # also named <tagname>_id, and an album join column (album_id).
    #
    # The Albums table is special because it is built according to,
    # and stores, the file system location (the album title is not
    # enough to group tracks, there could be many albums with the same title).
    def _createsqdb(self):
        c = self._conn.cursor()
        try:
            c.execute('''DROP TABLE albums''')
            c.execute('''DROP TABLE tracks''')
        except:
            pass
        c.execute(
            '''CREATE TABLE albums (album_id INTEGER PRIMARY KEY, artist_id INT,
            albtitle TEXT, albfolder TEXT, albdate TEXT, albarturi TEXT)''')

        tracksstmt = '''CREATE TABLE tracks
        (docidx INT, album_id INT, trackno INT, title TEXT'''

        for tb in _tagtotable.values():
            try:
                c.execute('DROP TABLE ' + tb)
            except:
                pass
            stmt = 'CREATE TABLE ' + tb + \
               ' (' + _clid(tb) + ' INTEGER PRIMARY KEY, value TEXT)'
            c.execute(stmt)
            tracksstmt += ',' + _clid(tb) + ' INT'

        tracksstmt += ')'
        c.execute(tracksstmt)
    

    # Insert new value if not existing, return rowid of new or existing row
    def _auxtableinsert(self, tb, value):
        c = self._conn.cursor()
        stmt = 'SELECT ' + _clid(tb) + ' FROM ' + tb + ' WHERE value = ?'
        c.execute(stmt, (value,))
        r = c.fetchone()
        if r:
            rowid = r[0]
        else:
            stmt = 'INSERT INTO ' + tb + '(value) VALUES(?)'
            c.execute(stmt, (value,))
            rowid = c.lastrowid
        return rowid


    # tracknos like n/max are now supposedly processed by rclaudio and
    # should not arrive here, but let's play it safe.
    def _tracknofordoc(self, doc):
        try:
            return int(doc.tracknumber.split('/')[0])
        except:
            return 0

    # Create album record if needed.
    # The albums table is special, can't use auxtableinsert()
    def _maybecreatealbum(self, c, doc):
        folder = docfolder(doc).decode('utf-8', errors = 'replace')
        album = getattr(doc, 'album', None)
        if not album:
            album = os.path.basename(folder)
            #uplog("Using %s for alb: mime %s title %s" %
            #(album,doc.mtype, doc.url))
        if doc.albumartist:
            albartist_id = self._auxtableinsert('artist', doc.albumartist)
        else:
            albartist_id = None
        c.execute('''SELECT album_id, artist_id FROM albums
        WHERE albtitle = ? AND albfolder = ?''', (album, folder))
        r = c.fetchone()
        if r:
            album_id = r[0]
            albartist_id = r[1]
        else:
            c.execute('''INSERT INTO albums(albtitle, albfolder, artist_id,
            albdate, albarturi)
            VALUES (?,?,?,?,?)''', (album, folder, albartist_id, doc.date,
                                    doc.albumarturi))
            album_id = c.lastrowid
        return album_id, albartist_id


    # Create the db and fill it up with the values we need, taken out of
    # the recoll records list
    def _recolltosql(self, docs):
        start = timer()

        self._createsqdb()

        c = self._conn.cursor()
        maxcnt = 0
        totcnt = 0
        for docidx in range(len(docs)):
            doc = docs[docidx]
            totcnt += 1

            if totcnt % 1000 == 0:
                time.sleep(0)
            
            # No need to include non-audio types in the visible tree.
            if doc.mtype not in audiomtypes or doc.mtype == 'inode/directory':
                continue

            album_id, albartist_id = self._maybecreatealbum(c, doc)
            
            trackno = self._tracknofordoc(doc)
            
            # Set base values for column names, values list,
            # placeholders
            columns = 'docidx,album_id,trackno,title'
            values = [docidx, album_id, trackno, doc.title]
            placehold = '?,?,?,?'
            # Append data for each auxiliary table if the doc has a value
            # for the corresponding field (else let SQL set a dflt/null value)
            for tb, rclfld in self._tabtorclfield:
                value = getattr(doc, rclfld, None)
                if not value:
                    continue
                rowid = self._auxtableinsert(tb, value)
                columns += ',' + _clid(tb)
                values.append(rowid)
                placehold += ',?'

            # Create the main record in the tracks table.
            stmt='INSERT INTO tracks(' + columns + ') VALUES(' + placehold + ')'
            c.execute(stmt, values)
            #uplog(doc.title)

            # If the album had no artist yet, set it from the track
            # artist.  Means that if tracks for the same album have
            # different artist values, we arbitrarily use the first
            # one.
            if not albartist_id:
                lcols = columns.split(',')
                try:
                    i = lcols.index('artist_id')
                    artist_id = values[i]
                    stmt = 'UPDATE albums SET artist_id = ? WHERE album_id = ?'
                    c.execute(stmt, (artist_id, album_id))
                except:
                    pass
        ## End Big doc loop
        
        self._conn.commit()
        end = timer()
        uplog("recolltosql: processed %d docs in %.2f Seconds" %
              (totcnt, end-start))


    # Create our top-level directories, with fixed entries, and stuff from
    # the tags tables
    def rootentries(self, pid):
        c = self._conn.cursor()
        c.execute("SELECT COUNT(*) from albums")
        nalbs = str(c.fetchone()[0])
        c.execute("SELECT COUNT(*) from tracks")
        nitems = str(c.fetchone()[0])
        entries = [rcldirentry(pid + 'albums', pid, nalbs + ' albums'),
                   rcldirentry(pid + 'items', pid, nitems + ' items')]
        for tt in sorted(_tagtotable.keys()):
            entries.append(rcldirentry(pid + '=' + tt , pid, tt))
        return entries


    # Return the list of tags which have multiple values inside the
    # input list of docids.
    def _subtreetags(self, docidsl):
        docids = ','.join([str(i) for i in docidsl])
        uplog("subtreetags, docids %s" % docids)
        c = self._conn.cursor()
        tags = []
        for tt,tb in _tagtotable.items():
            stmt = 'SELECT COUNT(DISTINCT ' + _clid(tb) + \
                   ') FROM tracks WHERE docidx IN (' + docids + ')'
            uplog("subtreetags: executing: <%s>" % stmt)
            c.execute(stmt)
            cnt = c.fetchone()[0]
            uplog("Found %d distinct values for %s" % (cnt, tb))
            if cnt > 1:
                tags.append(tt)
        return tags


    # Build a list of track directory entries for an SQL statement
    # which selects docidxs (SELECT docidx,... FROM tracks WHERE...)
    def _trackentriesforstmt(self, stmt, values, pid):
        c = self._conn.cursor()
        c.execute(stmt, values)
        entries = [rcldoctoentry(pid + '$i' + str(r[0]),
                                 pid, self._httphp, self._pprefix,
                                 self._rcldocs[r[0]]) for r in c]
        if PY3:
            return sorted(entries, key=cmpentries)
        else:
            return sorted(entries, cmp=cmpentries)
    

    # Return a list of trackids as selected by the current
    # path <selwhere> is like: WHERE col1_id = ? AND col2_id = ? [...], and
    # <values> holds the corresponding values
    def _docidsforsel(self, selwhere, values):
        c = self._conn.cursor()
        stmt = 'SELECT docidx FROM tracks ' + selwhere + ' ORDER BY trackno'
        uplog("docidsforsel: executing <%s> values %s" % (stmt, values))
        c.execute(stmt, values)
        return [r[0] for r in c.fetchall()]


    def _trackentriesforalbum(self, albid, pid):
        stmt = 'SELECT docidx FROM tracks WHERE album_id = ? ORDER BY trackno'
        return self._trackentriesforstmt(stmt, (albid,), pid)


    def _direntriesforalbums(self, pid, where):
        entries = []
        c = self._conn.cursor()
        if not where:
            where = ' LEFT JOIN artist ON artist.artist_id = albums.artist_id '
            frm = ' FROM albums '
        else:
            where += ' AND artist.artist_id = albums.artist_id '
            frm = ' FROM albums,artist '

        stmt = 'SELECT album_id, albtitle, albarturi, albdate, artist.value' + \
               frm + where + ' ORDER BY albtitle'
        uplog('direntriesforalbums: %s' % stmt)
        c.execute(stmt)
        for r in c:
            id = pid + '$' + str(r[0])
            entries.append(
                rcldirentry(id, pid, r[1], arturi=r[2], date=r[3],artist=r[4], 
                            upnpclass='object.container.album.musicAlbum'))
        return entries


    # This is called when an 'albums' element is encountered in the
    # selection path. i is the index of the albums element. The tree under
    # albums has a well defined structure: ql=len(qpath), we have an
    # albums list if i is the last element (i == ql-1), an album track
    # list for i == ql-2 (we then have an albid at ql-1), and a 'Complete
    # album' query if i == ql-3 (...$albums$xxx$showca)
    def _tagsbrowsealbums(self, pid, qpath, i, selwhere, values):
        uplog("_tagsbrowsealbums: pid %s qpath %s i %s selwhere %s values %s" %
              (pid, qpath, i, selwhere, values))
        c = self._conn.cursor()
        docidsl = self._docidsforsel(selwhere, values)
        entries = []
        if i == len(qpath)-1:
            albidsl = self._subtreealbums(docidsl)
            albids = ','.join([str(a) for a in albidsl])
            where = ' WHERE album_id in (' + albids + ') '
            entries = self._direntriesforalbums(pid, where)
        elif i == len(qpath)-2:
            albid = int(qpath[-1])
            docids = ','.join([str(i) for i in docidsl])
            stmt = 'SELECT COUNT(docidx) FROM tracks WHERE album_id = ?'
            c.execute(stmt, (albid,))
            r = c.fetchone()
            ntracks = int(r[0])
            stmt = 'SELECT docidx FROM tracks ' + \
                   'WHERE album_id = ? AND docidx IN ('+ docids + ')'
            entries = self._trackentriesforstmt(stmt, (albid,), pid)
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
            entries = self._trackentriesforalbum(albid, pid)
        
        return entries


    # This is called when an 'items' element is encountered in the
    # selection path. We just list the selected tracks
    # TBD: need Complete Album here too ?
    def _tagsbrowseitems(self, pid, qpath, i, selwhere, values):
        stmt = 'SELECT docidx FROM tracks ' + selwhere
        return self._trackentriesforstmt(stmt, values, pid)


    # Return all albums ids to which any of the currently selected tracks
    # (designated by a docid set) belong
    def _subtreealbums(self, docidsl):
        docids = ','.join([str(r) for r in docidsl])
        albids = []
        stmt = 'SELECT album_id from tracks where docidx IN (' \
               + docids + ') ' + 'GROUP BY album_id'
        c = self._conn.cursor()
        uplog('subtreealbums: executing %s' % stmt)
        c.execute(stmt)
        for r in c:
            albids.append(r[0])
        uplog('subtreealbums: returning %s' % albids)
        return albids
    

    # Main browsing routine. Given an objid, translate it into a select
    # statement, plus further processing, and return the corresponding
    # records
    def _tagsbrowse(self, pid, qpath, flag):
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
                col = _tagtotable[elt[1:]] 

            # Detect the special values: albums items etc. here. Their
            # presence changes how we process the rest (showing tracks and
            # albums and not dealing with other tags any more)
            if elt == 'albums':
                return self._tagsbrowsealbums(pid, qpath, i, selwhere, values)
            elif elt == 'items':
                return self._tagsbrowseitems(pid, qpath, i, selwhere, values)
            
            selwhere = selwhere + ' AND ' if selwhere else ' WHERE '
            if i == qlen - 1:
                # We want to display all unique values for the column
                # artist.artist_id, artist.value
                selwhat = col + '.' + _clid(col) + ', ' + col + '.value'
                # tracks.artist_id = artist.artist_id
                selwhere += 'tracks.' + _clid(col) + ' = ' + col + \
                            '.' + _clid(col)
            else:
                # Look at the value specified for the =xx column. The
                # selwhat value is only used as a flag
                selwhat = 'tracks.docidx'
                selwhere += 'tracks.' + _clid(col) + ' =  ?'
                i += 1
                values.append(int(qpath[i]))
            i += 1
            

        # TBD: Need a ">> Complete Album" entry if there is a single
        # album, no subqs and not all the tracks are listed
        entries = []
        if selwhat == 'tracks.docidx':
            docids = self._docidsforsel(selwhere, values)
            albids = self._subtreealbums(docids)
            subqs = self._subtreetags(docids)
            if len(albids) > 1:
                id = pid + '$albums'
                entries.append(rcldirentry(id, pid, str(len(albids)) +
                                           ' albums'))
                if subqs:
                    id = pid + '$items'
                    entries.append(rcldirentry(id,pid, str(len(docids)) +
                                               ' items'))
            elif len(albids) == 1 and subqs:
                id = pid + '$items'
                entries.append(rcldirentry(id,pid, str(len(docids)) + ' items'))

            if not subqs:
                for docidx in docids:
                    id = pid + '$*i' + str(docidx)
                    entries.append(rcldoctoentry(id, pid, self._httphp,
                                                 self._pprefix,
                                                 self._rcldocs[docidx]))
                    if PY3:
                        entries = sorted(entries, key=cmpentries)
                    else:
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
                   " GROUP BY tracks." + _clid(col) + \
                   " ORDER BY value"
            uplog("tagsbrowse: executing <%s> values %s" % (stmt,values))
            c = self._conn.cursor()
            c.execute(stmt, values)
            for r in c:
                id = pid + '$' + str(r[0])
                entries.append(rcldirentry(id, pid, r[1]))
        return entries


    # Browse the top-level tree named like 'xxx albums'. There are just 2
    # levels: the whole albums list, then for each entry the specified
    # albums track list
    def _albumsbrowse(self, pid, qpath, flag):
        c = self._conn.cursor()
        entries = []
        if len(qpath) == 1:
            entries = self._direntriesforalbums(pid, '')
        elif len(qpath) == 2:
            e1 = qpath[1]
            album_id = int(e1)
            entries = self._trackentriesforalbum(album_id, pid)
        else:
            raise Exception("Bad path in album tree (too deep): <%s>" % qpath)

        return entries


    # Top level browse routine. Handle the special cases and call the
    # appropriate worker routine.
    def browse(self, pid, flag):
        idpath = pid.replace(g_myprefix, '', 1)
        uplog('tags:browse: idpath <%s>' % idpath)
        entries = []
        qpath = idpath.split('$')
        if idpath.startswith('items'):
            stmt = 'SELECT docidx FROM tracks'
            entries = self._trackentriesforstmt(stmt, (), pid)
        elif idpath.startswith('albums'):
            entries = self._albumsbrowse(pid, qpath, flag)
        elif idpath.startswith('='):
            entries = self._tagsbrowse(pid, qpath, flag)
        else:
            raise Exception('Bad path in tags tree (start): <%s>' % idpath)
        return entries









############ Misc test/trial code, not used by uprcl ########################

def misctries():
    c = self._conn.cursor()
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
    
