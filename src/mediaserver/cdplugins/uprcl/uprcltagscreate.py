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
import uprclinit

# Tags for which we create auxiliary tables for facet descent. 
#
# The key is the presentation name (container title). The value is the
# auxiliary table name, used also as base for unique id and join
# columns (with _id) appended, and is also currently the recoll field
# name (with a provision to differ if needed, thanks to the
# currently empty _coltorclfield dict).
#
# The keys must stay same as what Minim uses as we filter them with
# indexTags from minim config
#
# Note: artist is actually doc.albumartist if set, else doc.artist and
# is processed separately
_tagtotable = {
    'AlbumArtist' : 'albumartist',
    'All Artists' : 'allartists',
    'Artist' : 'artist',
    'Comment' : 'comment',
    'Composer' : 'composer',
    'Conductor' : 'conductor',
    'Date' : 'date',
    'Genre' : 'genre',
    'Group' : 'contentgroup',
    'Label' : 'label',
    'Lyricist' : 'lyricist',
    'Orchestra' : 'orchestra',
    'Performer' : 'performer',
    }

_tagdisplaytag = {}

# Translation only used when fetching fields from the recoll
# record. None at the moment
_coltorclfield = {
    'allartists' : 'artist'
    }


def _clid(table):
    return table + '_id'

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
# group tracks, there could be many albums with the same title). Also
# we do tricks with grouping discs into albums etc.
def _createsqdb(conn):
    c = conn.cursor()
    try:
        c.execute('''DROP TABLE albums''')
        c.execute('''DROP TABLE tracks''')
    except:
        pass

    c.execute(
        'CREATE TABLE albums (album_id INTEGER PRIMARY KEY,'
        # artist_id is for an album_artist, either explicit from an
        # albumartist attribute or implicit if all tracks have the
        # same artist attribute
        'artist_id INT,'
        'albtitle TEXT, albfolder TEXT, albdate TEXT, albarturi TEXT,'
        # albalb is non null only the elements of a merged album, and
        # is the album_id id for the parent. It is set by the album
        # merging pass.
        'albalb INT,'
        # albtdisc is set from the tracks DISCNUMBER tag or
        # equivalent, while initially walking the docs. It signals
        # candidates for merging. album/discs with albtdisc set are
        # not selected for display in album lists, as they are
        # supposedly represented by their parent album. The field is
        # reset to null for albums with a discnumber which do not end
        # up being merged.
        'albtdisc INT,'
        # Temp columns while creating the table for deciding if we
        # have a uniform artist or not. albartok is initially true,
        # set to false as soon as we find a differing track artist.
        'albtartist INT,'
        'albartok INT'
        ')')

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
    #uplog("_auxtableinsert [%s] -> [%s]" % (tb, value))
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
        #uplog("_tracknofordoc: doc.tracknumber %s title %s url %s" %
        #      (doc.tracknumber, doc.title, doc.url))
        return 1


# Detect '[disc N]' or '(disc N)' or ', disc N' at the end of an album title
_albtitdnumre = "(.*)(\[disc ([0-9]+)\]|\(disc ([0-9]+)\)|,[ ]*disc ([0-9]+))$"
_albtitdnumexp = re.compile(_albtitdnumre, flags=re.IGNORECASE)
_folderdnumre = "(cd|disc)[ ]*([0-9]+)(.*)"
_folderdnumexp = re.compile(_folderdnumre, flags=re.IGNORECASE)

# Create album record for track if needed (not already there).
# The albums table is special, can't use auxtableinsert()
def _maybecreatealbum(conn, doc, trackartid):
    c = conn.cursor()
    folder = docfolder(doc).decode('utf-8', errors = 'replace')

    album = getattr(doc, 'album', None)
    if not album:
        album = os.path.basename(folder)
        #uplog("Using %s for alb MIME %s title %s" % (album,doc.mtype,doc.url))

    if doc.albumartist:
        albartist_id = _auxtableinsert(conn, 'artist', doc.albumartist)
    else:
        albartist_id = None

    # See if there is a discnumber, either explicit or from album
    # title
    discnumber = None
    if doc.discnumber:
        try:
            discnumber = int(doc.discnumber)
            #uplog("discnumber %d folder %s" % (discnumber, folder))
        except:
            pass

    # Look for a disc number at the end of the album title. If it's
    # there and it's the same as the discnumber attribute, we fix the
    # title, else we leave it alone (and the merge will later fail for
    # differing titles)
    m = _albtitdnumexp.search(album)
    if m:
        for i in (3,4,5):
            if m.group(i):
                adiscnumber = int(m.group(i))
                if not discnumber:
                    discnumber = adiscnumber
                if adiscnumber == discnumber:
                    album = m.group(1)
                break
            
    if not discnumber:
        m = _folderdnumexp.search(os.path.basename(folder))
        if m:
            discnumber = int(m.group(2))
        
    # See if this albumdisc already exists (created for a previous track)
    stmt = '''SELECT album_id, artist_id, albartok, albtartist FROM albums
       WHERE albtitle = ? AND albfolder = ?'''
    wcols = [album, folder]
    if discnumber:
        stmt += ' AND albtdisc = ?'
        wcols.append(discnumber)
    #uplog("maybecreatealbum: %s %s" % (stmt, cols))
    c.execute(stmt, wcols)
    r = c.fetchone()
    if r:
        album_id = r[0]
        albartist_id = r[1]
        albartok = r[2]
        albtartist = r[3]
        #uplog("albartist_id %s albartok %s" % (albartist_id, albartok))
        if not albartist_id and albartok:
            # If we still have a possible common artist, check new track
            if albtartist:
                if trackartid != albtartist:
                    #uplog("Unsetting albartok albid %d"%album_id)
                    c.execute('''UPDATE albums SET albartok = 0
                        WHERE album_id = ?''', (album_id,))
            else:
                #uplog("Setting albtartist albid %d"%album_id)
                c.execute('''UPDATE albums SET albtartist = ?
                    WHERE album_id = ?''', (trackartid, album_id))
    else:
        c.execute('''INSERT INTO
            albums(albtitle, albfolder, artist_id, albdate, albarturi,
            albtdisc, albartok, albtartist) VALUES (?,?,?,?,?,?,?,?)''',
                  (album, folder, albartist_id, doc.date,
                   doc.albumarturi, discnumber, 1, trackartid))
        album_id = c.lastrowid
        #uplog("Created album %d %s disc %s artist %s folder %s" %
        #      (album_id, album, discnumber, albartist_id, folder))

    return album_id, albartist_id


def _artistvalue(conn, artid):
    c = conn.cursor()
    c.execute('''SELECT value FROM artist where artist_id = ?''', (artid,))
    r = c.fetchone()
    if r:
        return r[0]
    else:
        return None


# After the pass on tracks, look for all albums where the album artist
# is not set but all the tracks have the same artist, and set the
# album artist.
def _setalbumartists(conn):
    c = conn.cursor()

    # Get all albums without an albumartist where the
    # same_track_artist flag is on
    c.execute('''SELECT album_id, albtartist FROM albums
        WHERE albartok = 1 AND artist_id IS NULL''')
    c1 = conn.cursor()
    for r in c:
        if r[1]:
            #uplog("alb %d set artist %s" % (r[0], _artistvalue(conn, r[1])))
            c1.execute('''UPDATE albums SET artist_id = ?
                WHERE album_id = ?''', (r[1], r[0]))


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


def _membertotopalbum(conn, memberalbid):
    c = conn.cursor()
    c.execute('''SELECT * FROM albums WHERE album_id = ?''', (memberalbid,))
    cols = [desc[0] for desc in c.description]
    # Get array of column values, and set primary key and albtdisc to
    # None before inserting the copy
    tdiscindex = cols.index('albtdisc')
    v = [e for e in c.fetchone()]
    v[0] = None
    v[tdiscindex] = None
    c.execute('''INSERT INTO albums VALUES (%s)''' % ','.join('?'*len(v)),  v)
    return c.lastrowid


# Only keep albums in the same folder or in a sibling folder
def _mergealbumsfilterfolders(folder, rows, colidx):
    rows1 = []
    for row in rows:
        if row[colidx] == folder or \
               os.path.dirname(row[colidx]) == os.path.dirname(folder):
            rows1.append(row)
    return rows1


## TBD: folder match filter
def _createmergedalbums(conn):
    c = conn.cursor()

    # Remember already merged
    merged = set()

    # All candidates for merging: albums with a disc number.
    c.execute('''SELECT album_id, albtitle, artist_id, albfolder FROM albums
      WHERE albalb IS NULL AND albtdisc IS NOT NULL''')
    c1 = conn.cursor()
    for r in c:
        albid = r[0]
        if albid in merged:
            continue
        albtitle = r[1]
        artist = r[2]
        folder = r[3]

        #uplog("_createmergedalbums: albid %d artist_id %s albtitle %s" %
        #      (albid, artist, albtitle))

        # Look for albums not already in a group, with the same title and artist
        if artist:
            c1.execute('''SELECT album_id, albtdisc, albfolder FROM albums
                WHERE albtitle = ? AND artist_id = ?
                AND albalb is NULL AND albtdisc IS NOT NULL''',
                       (albtitle, artist))
        else:
            c1.execute('''SELECT album_id, albtdisc, albfolder FROM albums
                WHERE albtitle = ? AND artist_id IS NULL
                AND albalb is NULL AND albtdisc IS NOT NULL''',
                       (albtitle,))

        rows = c1.fetchall()
        rows1 = _mergealbumsfilterfolders(folder, rows, 2)

        #uplog("_createmergedalbums: got %d possible(s) title %s" %
        #      (len(rows1), albtitle))

        if len(rows1) > 1:
            albids = [row[0] for row in rows1]
            dnos = sorted([row[1] for row in rows1])
            if not _checkseq(dnos):
                uplog("mergealbums: not merging bad seq %s for albtitle %s " %
                      (dnos, albtitle))
                c1.execute('''UPDATE albums SET albtdisc = NULL
                  WHERE album_id in (%s)''' % ','.join('?'*len(albids)), albids)
                continue

            # Create record for whole album by copying the first
            # record, setting its album_id and albtdisc to NULL
            topalbid = _membertotopalbum(conn, albids[0])

            # Update all album disc members with the top album id
            values = [topalbid,] + albids
            c1.execute('''UPDATE albums SET albalb = ?
                WHERE album_id in (%s)''' % ','.join('?'*len(albids)), values)
            merged.update(albids)
            #uplog("_createmergedalbums: merged: %s" % albids)
            
        elif len(rows1) == 1:
            # Album with a single disc having a discnumber. Just unset
            # the discnumber, we won't use it and its presence would
            # prevent the album from showing up. Alternatively we
            # could set albalb = album_id?
            #uplog("Setting albtdisc to NULL albid %d" % albid)
            c1.execute('''UPDATE albums SET albtdisc = NULL
                WHERE album_id= ?''', (albid,))


# Set up the tag display name to column to recoll field translations
# and get rid of tags which are not going to display (keep just
# indexTags)
def _prepareTags():
    global _tabtorclfield
    global _tagdisplaytag
    _tabtorclfield = []

    indextagsp = []
    if uprclinit.g_minimconfig:
        indextagsp = uprclinit.g_minimconfig.getindextags()
    if not indextagsp:
        indextagsp = [('Artist',''), ('Date',''), ('Genre',''), ('Composer','')]
    indextags = []
    for v,d in indextagsp:
        indextags.append(v)
        _tagdisplaytag[v] = d if d else v
        
    # Compute an array of (table name, recoll field) translations. Most
    # often they are identical.
    todelete = []
    for nm, tb in _tagtotable.items():
        if nm not in indextags:
            todelete.append(nm)
            continue
        if tb in _coltorclfield:
            rclfld = _coltorclfield[tb]
        else:
            rclfld = tb
        uplog("recolltosql: using rclfield [%s] for sqlcol [%s]"% (rclfld, tb))
        _tabtorclfield.append((tb, rclfld))
    for nm in todelete:
        del _tagtotable[nm]


# Create the db and fill it up with the values we need, taken out of
# the recoll records list
def recolltosql(conn, docs):
    global _tabtorclfield
    start = timer()

    _createsqdb(conn)
    _prepareTags()

    maxcnt = 0
    totcnt = 0
    c = conn.cursor()
    for docidx in range(len(docs)):
        doc = docs[docidx]
        totcnt += 1

        if totcnt % 1000 == 0:
            time.sleep(0)
        
        # No need to include non-audio or non-tagged types
        if doc.mtype not in audiomtypes or doc.mtype == 'inode/directory' \
               or doc.mtype == 'audio/x-mpegurl':
            continue

        # Do the artist apart from the other attrs, as we need the
        # value for album creation.
        if doc.albumartist:
            trackartid = _auxtableinsert(conn, 'artist', doc.albumartist)
        elif doc.artist:
            trackartid = _auxtableinsert(conn, 'artist', doc.artist)
        else:
            trackartid = None
        album_id, albartist_id = _maybecreatealbum(conn, doc, trackartid)
        
        trackno = _tracknofordoc(doc)
        
        # Set base values for column names, values list,
        # placeholders
        columns = ['docidx', 'album_id', 'trackno', 'title', 'artist_id']
        values = [docidx, album_id, trackno, doc.title, trackartid]
        placehold = ['?', '?', '?', '?', '?']
        # Append data for each auxiliary table if the doc has a value
        # for the corresponding field (else let SQL set a dflt/null value)
        for tb, rclfld in _tabtorclfield:
            if tb == 'artist': # already done
                continue
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

    ## End Big doc loop

    _setalbumartists(conn)
    _createmergedalbums(conn)
    conn.commit()
    end = timer()
    uplog("recolltosql: processed %d docs in %.2f Seconds" %
          (totcnt, end-start))

