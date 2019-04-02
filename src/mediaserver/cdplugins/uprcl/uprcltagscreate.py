# Copyright (C) 2017-2019 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU Lesser General Public License as published by
#   the Free Software Foundation; either version 2.1 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

import os
import time
import re

from timeit import default_timer as timer
from uprclutils import audiomtypes, docfolder, uplog

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

# Create an empty db.
#
# There is then one table for each tag (Artist, Genre, Date,
# etc.).  Each tag table has 2 columns: <tagname>_id and value.
# 
# The tracks table is the "main" table, and has a record for each
# track, with a title column, and one join column for each tag, 
# also named <tagname>_id, and an album join column (album_id).
#
# The Albums table is special because it is built according to, and
# stores, the file system location (the album title is not enough to
# group tracks, there could be many albums with the same title).
def _createsqdb(conn):
    c = conn.cursor()
    try:
        c.execute('''DROP TABLE albums''')
        c.execute('''DROP TABLE tracks''')
    except:
        pass

    # - albtdisc is set from the tracks DISCNUMBER tag or equivalent, while
    #   initially walking the docs.
    # - albalb is the id for the merged album, set by the album merging
    #   pass, it is non null only the elements of a merged album.
    c.execute(
        '''CREATE TABLE albums (album_id INTEGER PRIMARY KEY, artist_id INT,
        albtitle TEXT, albfolder TEXT, albdate TEXT, albarturi TEXT,
        albalb INT, albtdisc INT)''')

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
def _auxtableinsert(conn, tb, value):
    c = conn.cursor()
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
def _tracknofordoc(doc):
    try:
        return int(doc.tracknumber.split('/')[0])
    except:
        return 0


_albtitdnumre = "(.*)(\[disc ([0-9]+)\]|\(disc ([0-9]+)\)|,[ ]*disc ([0-9]+))$"
_albtitdnumexp = re.compile(_albtitdnumre)

# Create album record for track if needed (not already there).
# The albums table is special, can't use auxtableinsert()
def _maybecreatealbum(conn, doc):
    c = conn.cursor()
    folder = docfolder(doc).decode('utf-8', errors = 'replace')
    album = getattr(doc, 'album', None)
    if not album:
        album = os.path.basename(folder)
        uplog("Using %s for alb: mime %s title %s" % (album,doc.mtype, doc.url))
    if doc.albumartist:
        albartist_id = _auxtableinsert(conn, 'artist', doc.albumartist)
    else:
        albartist_id = None

    # See if there is a discnumber, either explicit or from album title
    discnumber = None
    if doc.discnumber:
        try:
            discnumber = int(doc.discnumber)
            #uplog("discnumber %d folder %s" % (discnumber, folder))
        except:
            pass
    if not discnumber:
        m = _albtitdnumexp.search(album)
        if m:
            for i in (3,4,5):
                if m.group(i):
                    discnumber = m.group(i)
                    album = m.group(1)
                    break
        
    # See if this albumdisc already exists
    stmt = '''SELECT album_id, artist_id FROM albums
      WHERE albtitle = ? AND albfolder = ?'''
    cols = [album, folder]
    if discnumber:
        stmt += ' AND albtdisc = ?'
        cols.append(discnumber)
    #uplog("maybecreatealbum: %s %s" % (stmt, cols))
    c.execute(stmt, cols)
    r = c.fetchone()
    if r:
        album_id = r[0]
        albartist_id = r[1]
    else:
        uplog("Creating album")
        c.execute('''INSERT INTO
        albums(albtitle, albfolder, artist_id, albdate, albarturi, albtdisc)
        VALUES (?,?,?,?,?,?)''',
                  (album, folder, albartist_id, doc.date,
                   doc.albumarturi, discnumber))
        album_id = c.lastrowid

    return album_id, albartist_id

# Check that the numbers are sequential
def _checkseq(seq):
    num = seq[0]
    if not num:
        return False
    for e in seq[1:]:
        if e != num + 1:
            return False
        num = e
    return True

def _createmergedalbums(conn):
    ## TBD: checks on artists being the same and, later folder match filter
    c = conn.cursor()

    # Get all albums with a disc number. They are the candidates for merging.
    c.execute('''SELECT album_id, albtdisc, albtitle FROM albums
      WHERE albalb IS NULL AND albtdisc IS NOT NULL''')
    for r in c:
        c1 = conn.cursor()
        # Look for albums not already in a group, with the same title.
        c1.execute('''SELECT album_id, albtdisc FROM albums
          WHERE albtitle = ? AND albalb is NULL AND albtdisc IS NOT NULL''',
                   (r[2],))
        rows1 = c1.fetchall()
        if len(rows1) > 1:
            albids = [row[0] for row in rows1]
            dnos = sorted([row[1] for row in rows1])
            if not _checkseq(dnos):
                uplog("mergedalbums: not merging bad seq %s for albtitle %s " %
                      (dnos, r[2]))
                c1.execute('''UPDATE albums SET albtdisc = NULL
                  WHERE album_id in (%s)''' % ','.join('?'*len(albids)), albids)
                continue

            uplog("album_id: %s %d rows for albtitle %s" %
                  (r[0], len(rows1), r[2]))
            # for row in rows1: uplog("   Same group: id %s " % row[0])

            # Create record for whole album by copying the first
            # record, setting its its album_id and albtdisc to NULL
            # (album_id will be auto-set).
            c2 = conn.cursor()
            c2.execute('''SELECT * FROM albums WHERE album_id = ?''', (r[0],))
            v = [e for e in c2.fetchone()]
            v[0] = None
            c2.execute('''INSERT INTO albums
              VALUES (%s)''' % ','.join('?'*len(v)),  v)
            rowid = c2.lastrowid
            c2.execute('''UPDATE albums set albtdisc = NULL WHERE album_id=?''',
                       (rowid,))

            # Update all album disc members with the top album id
            uplog("Updating album_ids: %s" % albids)
            values = [rowid,] + albids
            c2.execute('''UPDATE albums SET albalb = ?
              WHERE album_id in (%s)''' % ','.join('?'*len(albids)), values)

        elif len(rows1) == 1:
            # Album with a single disc having a discnumber. Just unset
            # the discnumber, we won't use it and its presence would
            # prevent the album from showing up. Alternatively we
            # could set albalb = album_id?
            #uplog("Setting albtdisc to NULL albid %d" % r[0])
            c1.execute('''UPDATE albums SET albtdisc = NULL WHERE album_id= ?''',
                       (r[0],))
            

# Create the db and fill it up with the values we need, taken out of
# the recoll records list
def recolltosql(conn, docs):
    global _tabtorclfield
    start = timer()

    # Compute an array of (table name, recoll field) translations. Most
    # often they are identical.
    _tabtorclfield = []
    for tb in _tagtotable.values():
        if tb in _coltorclfield:
            rclfld = _coltorclfield[tb]
        else:
            rclfld = tb
        _tabtorclfield.append((tb, rclfld))

    _createsqdb(conn)

    maxcnt = 0
    totcnt = 0
    c = conn.cursor()
    for docidx in range(len(docs)):
        doc = docs[docidx]
        totcnt += 1

        if totcnt % 1000 == 0:
            time.sleep(0)
        
        # No need to include non-audio types in the visible tree.
        if doc.mtype not in audiomtypes or doc.mtype == 'inode/directory':
            continue

        album_id, albartist_id = _maybecreatealbum(conn, doc)
        
        trackno = _tracknofordoc(doc)
        
        # Set base values for column names, values list,
        # placeholders
        columns = ['docidx', 'album_id', 'trackno', 'title']
        values = [docidx, album_id, trackno, doc.title]
        placehold = ['?', '?', '?', '?']
        # Append data for each auxiliary table if the doc has a value
        # for the corresponding field (else let SQL set a dflt/null value)
        for tb, rclfld in _tabtorclfield:
            value = getattr(doc, rclfld, None)
            if not value:
                continue
            rowid = _auxtableinsert(conn, tb, value)
            columns.append(_clid(tb))
            values.append(rowid)
            placehold.append('?')

        # Create the main record in the tracks table.
        stmt='INSERT INTO tracks(' + ','.join(columns) + \
              ') VALUES(' + ','.join(placehold) + ')'
        c.execute(stmt, values)
        #uplog(doc.title)

        # If the album had no artist yet, set it from the track
        # artist.  Means that if tracks for the same album have
        # different artist values, we arbitrarily use the first
        # one.
        if not albartist_id:
            try:
                i = columns.index('artist_id')
                artist_id = values[i]
                stmt = 'UPDATE albums SET artist_id = ? WHERE album_id = ?'
                c.execute(stmt, (artist_id, album_id))
            except:
                pass

    ## End Big doc loop

    _createmergedalbums(conn)
    conn.commit()
    end = timer()
    uplog("recolltosql: processed %d docs in %.2f Seconds" %
          (totcnt, end-start))

