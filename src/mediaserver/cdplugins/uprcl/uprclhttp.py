#!/usr/bin/env python
#
# Copyright (C) 2017 J.F.Dockes
#
# HTTP Range code:
#  Portions Copyright (C) 2009,2010  Xyne
#  Portions Copyright (C) 2011 Sean Goller
#  https://github.com/smgoller/rangehttpserver/blob/master/RangeHTTPServer.py
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

import SocketServer
import BaseHTTPServer
import SimpleHTTPServer
import os
import posixpath
import BaseHTTPServer
import urllib
import cgi
import shutil
import mimetypes
try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO

from uprclutils import uplog



__version__ = "0.1"

class RangeHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    """Simple HTTP request handler with GET and HEAD commands.

    This serves files from the current directory and any of its
    subdirectories.  The MIME type for files is determined by
    calling the .guess_type() method.

    The GET and HEAD requests are identical except that the HEAD
    request omits the actual contents of the file.

    """

    server_version = "RangeHTTP/" + __version__

    def do_GET(self):
        """Serve a GET request."""
        f, start_range, end_range = self.send_head()
        if f:
            uplog("do_GET: Got (%d,%d)" % (start_range,end_range))
            f.seek(start_range, 0)
            chunk = 0x1000
            total = 0
            while chunk > 0:
                if start_range + chunk > end_range:
                    chunk = end_range - start_range
                try:
                    self.wfile.write(f.read(chunk))
                except:
                    break
                total += chunk
                start_range += chunk
            f.close()

    def do_HEAD(self):
        """Serve a HEAD request."""
        f, start_range, end_range = self.send_head()
        if f:
            f.close()

    def send_head(self):
        """Common code for GET and HEAD commands.

        This sends the response code and MIME headers.

        Return value is either a file object (which has to be copied
        to the outputfile by the caller unless the command was HEAD,
        and must be closed by the caller under all circumstances), or
        None, in which case the caller has nothing further to do.

        """
        uplog("HTTP: path: %s" % self.path)
        path = self.translate_path(self.path)
        if not path or not os.path.exists(path):
            self.send_error(404)
            return (None, 0, 0)

        if not os.path.isfile(path):
            self.send_error(405)
            return (None, 0, 0)

        f = None
        ctype = self.guess_type(path)
        try:
            f = open(path, 'rb')
        except:
            self.send_error(404, "File not found")
            return (None, 0, 0)

        if "Range" in self.headers:
            self.send_response(206)
        else:
            self.send_response(200)

        self.send_header("Content-type", ctype)
        fs = os.fstat(f.fileno())
        size = int(fs[6])
        start_range = 0
        end_range = size
        self.send_header("Accept-Ranges", "bytes")
        if "Range" in self.headers:
            s, e = self.headers['range'][6:].split('-', 1)
            sl = len(s)
            el = len(e)
            if sl > 0:
                start_range = int(s)
                if el > 0:
                    end_range = int(e) + 1
            elif el > 0:
                ei = int(e)
                if ei < size:
                    start_range = size - ei
        self.send_header("Content-Range",
                         'bytes ' + str(start_range) + '-' +
                         str(end_range - 1) + '/' + str(size))
        self.send_header("Content-Length", end_range - start_range)
        self.send_header("Last-Modified", self.date_time_string(fs.st_mtime))
        self.end_headers()
        #uplog("Sending Bytes %d to %d" % (start_range, end_range))
        return (f, start_range, end_range)


    def translate_path(self, opath):
        path = urllib.unquote(opath)
        for p in self.uprclpathmap.itervalues():
            if path.startswith(p):
                return path
        uplog("HTTP: translate_path: %s not found in path map" % opath)
        return None

    def guess_type(self, path):
        """Guess the type of a file.

        Argument is a PATH (a filename).

        Return value is a string of the form type/subtype,
        usable for a MIME Content-type header.

        The default implementation looks the file's extension
        up in the table self.extensions_map, using application/octet-stream
        as a default; however it would be permissible (if
        slow) to look inside the data to make a better guess.

        """

        base, ext = posixpath.splitext(path)
        if ext in self.extensions_map:
            return self.extensions_map[ext]
        ext = ext.lower()
        if ext in self.extensions_map:
            return self.extensions_map[ext]
        else:
            return self.extensions_map['']

    if not mimetypes.inited:
        mimetypes.init() # try to read system mime.types
    extensions_map = mimetypes.types_map.copy()
    extensions_map.update({
        '': 'application/octet-stream', # Default
        '.mp4': 'video/mp4',
        '.ogg': 'video/ogg',
        })


class ThreadingSimpleServer(SocketServer.ThreadingMixIn,
                            BaseHTTPServer.HTTPServer):
    pass


def runHttp(host='', port=8080, pathmap={}):

    # Set pathmap as request handler class variable
    RangeHTTPRequestHandler.uprclpathmap = pathmap
    
    server = ThreadingSimpleServer((host, port), RangeHTTPRequestHandler)
    while 1:
        server.handle_request()
