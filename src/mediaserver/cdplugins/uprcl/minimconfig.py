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


    # Split on commas and colons, a common minim format and return as
    # list of pairs
    def minimsplitsplit(self, str):
        out = []
        if not str:
            return out
        str = str.replace('\\:', ':')
        lst = str.split(',')
        for e in lst:
            l = e.split(':')
            if len(l) == 0:
                a = ''
                b = ''
            elif len(l) == 1:
                a = l[0]
                b = ''
            else:
                a = l[0]
                b = l[1]
            out.append((a.strip(),b.strip()))
        return out


    def gettagaliases(self):
        aliases = []
        saliases = self.conf.get("minimserver.aliasTags")
        uplog("Minim:gettagaliases:in: [%s]" % saliases)
        lst = self.minimsplitsplit(saliases)
        for orig,target in lst:
            orig = orig.lower()
            target = target.lower()
            rep = True
            if target[0] == '-'[0]:
                rep = False
                target = target[1:]
            aliases.append((orig, target, rep))
        uplog("Minim:gettagaliases:out: %s" % aliases)
        return aliases

        
    def getindextags(self):
        indexTags = []
        sit = self.conf.get("minimserver.indexTags")
        uplog("Minim:getindextags:in: [%s]" % sit)
        if sit:
            indexTags = self.minimsplitsplit(sit)
        uplog("Minim:getindextags:out: %s" % indexTags)
        return indexTags

