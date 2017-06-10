##########################################################################
# Copyright 2009 Carlos Ribeiro
#
# This file is part of Radio Tray
#
# Radio Tray is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 1 of the License, or
# (at your option) any later version.
#
# Radio Tray is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Radio Tray.  If not, see <http://www.gnu.org/licenses/>.
#
##########################################################################
import urllib2

class DummyMMSHandler(urllib2.HTTPRedirectHandler):

    # Handle mms redirection, or let the standard code deal with it.
    def http_error_302(self, req, fp, code, msg, headers):
        if 'location' in headers:
            newurl = headers.getheaders('location')[0]
            if newurl.startswith('mms:'):
                raise urllib2.URLError("MMS REDIRECT:"+headers["Location"])
        return urllib2.HTTPRedirectHandler.http_error_302(self, req, fp, code,
                                                          msg, headers)
