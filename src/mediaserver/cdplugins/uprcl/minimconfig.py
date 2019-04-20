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
    def __init__(self, fn = ''):
        if fn:
            self.conf = conftree.ConfSimple(fn)
            uplog("Minim config read: contentDir: %s" %
                  self.conf.get("minimserver.contentDir"))
        else:
            self.conf = conftree.ConfSimple('/dev/null')
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
        lst = str.replace('\\:', ':').split(',')
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
        indextags = []
        sit = self.conf.get("minimserver.indexTags")
        uplog("Minim:getindextags:in: [%s]" % sit)
        if sit:
            indextags = self.minimsplitsplit(sit)
        uplog("Minim:getindextags:out: %s" % indextags)
        return indextags


    def getitemtags(self):
        itemtags = []
        sit = self.conf.get("minimserver.itemTags")
        uplog("Minim:getitemtags:in: [%s]" % sit)
        if sit:
            itemtags = [i.strip() for i in sit.split(',')]
        uplog("Minim:getindextags:out: %s" % itemtags)
        return itemtags


    def getcontentdirs(self):
        # minim uses '\\n' as a separator for directories (actual
        # backslash then n), not newline. Weird...
        cdirs = []
        s = self.conf.get("minimserver.contentDir")
        if s:
            cdirs = s.replace("\\n", "\n").split("\n")
        return cdirs

    
    def getsimplevalue(self, nm):
        s = self.conf.get(nm)
        if s:
            return s.strip()
        else:
            return s


    def getboolvalue(self, nm, dflt):
        val = self.getsimplevalue(nm)
        if val is None or val == '':
            return dflt
        if val == '0' or val.lower()[0] == 'f' or val.lower()[0] == 'n':
            return False
        else:
            return True

        
        
