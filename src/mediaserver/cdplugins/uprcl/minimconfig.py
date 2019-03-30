# Copyright (C) 2019 J.F.Dockes
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

import conftree
from uprclutils import uplog

class MinimConfig(object):
    def __init__(self, fn):
        self.fn = fn
        self.conf = conftree.ConfSimple(fn)
        uplog("Minim config read: contentDir: %s" %
              self.conf.get("minimserver.contentDir"))
        self.quotes = "\"'"
        self.escape = ''
        self.whitespace = ', '


    def getexcludepatterns(self):
        spats = self.conf.get("minimserver.excludePattern")
        if spats:
            lpats = conftree.stringToStrings(spats,
                                             quotes = self.quotes,
                                             escape = self.escape,
                                             whitespace = self.whitespace)
            spats = conftree.stringsToString(lpats)
        uplog("skippedNames from Minim excludePattern: %s" % spats)
        return spats
