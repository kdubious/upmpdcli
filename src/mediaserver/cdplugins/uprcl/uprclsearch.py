#!/usr/bin/env python
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
from __future__ import print_function

import re
import uprclfolders
from recoll import recoll

from uprclutils import uplog, stringToStrings, rcldoctoentry, cmpentries

def _getchar(s, i):
    if i < len(s):
        return i+1,s[i]
    else:
        return i,None

def _readword(s, i):
    w = ''
    for j in range(i, len(s)):
        if s[j].isspace():
            return j,w
        w += s[j]
    return j,w

# Called with '"' already read.

# Upnp search term strings are double quoted, but we should not take
# them as recoll phrases. We separate parts which are internally
# quoted, and become phrases, and lists of words which we interpret as
# an and search (comma-separated). Internal quotes come backslash-escaped
def _parsestring(s, i=0):
    uplog("parseString: input: <%s>" % s[i:])
    # First change '''"hello \"one phrase\"''' world" into
    #  '''hello "one phrase" world'''
    # Note that we can't handle quoted dquotes inside string
    str = ''
    escape = False
    instring = False
    for j in range(i, len(s)):
        if instring:
            if escape:
                if s[j] == '"':
                    str += '"'
                    instring = False
                else:
                    str += '\\' + s[j]
                escape = False
            else:
                if s[j] == '\\':
                    escape = True
                else:
                    str += s[j]

        else:
            if escape:
                str += s[j]
                escape = False
                if s[j] == '"':
                    instring = True
            else:
                if s[j] == '\\':
                    escape = True
                elif s[j] == '"':
                    j += 2
                    break
                else:
                    str += s[j]
                
    tokens = stringToStrings(str)
    return j, tokens

def _appendterms(out, v, field, oper):
    uplog("_appendterms: v %s field <%s> oper <%s>" % (v,field,oper))
    swords = ""
    phrases = []
    for w in v:
        if len(w.split()) == 1:
            if swords:
                swords += ","
            swords += w
        else:
            phrases.append(w)
    out.append(swords)
    for ph in phrases:
        out.append(field)
        out.append(oper)
        out.append('"' + ph + '"')
            
def upnpsearchtorecoll(s):
    uplog("upnpsearchtorecoll:in: <%s>" % s)

    s = re.sub('[\t\n\r\f ]+', ' ', s)

    out = []
    hadDerived = False
    i = 0
    field = ""
    oper = ""
    while True:
        i,c = _getchar(s, i)
        if not c:
            break
        #uplog("upnpsearchtorecoll: nextchar: <%s>" % c)

        if c.isspace():
            continue

        if c == "*":
            if (len(out) > 1 or (len(out) == 1 and not out[-1].isspace())) or \
                   (len(s[i:]) and not s[i:].isspace()):
                raise Exception("If * is used it must be the only input")
            out = ["mime:*"]
            break

        if c == '(' or c == ')': 
            out.append(c)
        elif c == '>' or c == '<' or c == '=':
            oper += c
        else:
            if c == '"':
                i,v = _parsestring(s, i)
                uplog("_parsestring ret: %s" % v)
                _appendterms(out, v, field, oper)
                oper = ""
                field = ""
                continue
            else:
                i -= 1
                i,w = _readword(s, i)
                #uplog("_readword returned <%s>" % w)

            if w == 'contains':
                out.append(':')
                oper = ':'
            elif w == 'doesNotContain':
                if len(out) < 1:
                    raise Exception("doesNotContain can't be the first word")
                out.insert(-1, "-")
                out.append(':')
                oper = ':'
            elif w == 'derivedFrom':
                hadDerived = True
                out.append(':')
                oper = ':'
            elif w == 'true':
                out.append('*')
                oper = ""
            elif w == 'false':
                out.append('xxxjanzocsduochterrrrm')
            elif w == 'exists':
                out.append(':')
                oper = ':'
            elif w == 'and':
                # Recoll has implied AND, but see next
                pass
            elif w == 'or':
                # Does not work because OR/AND priorities are reversed
                # between recoll and upnp. This would be very
                # difficult to correct, let's hope that the callers
                # use parentheses
                out.append('OR')
            else:
                try:
                    field = upnp2rclfields[w]
                except:
                    field = w
                out.append(field)
                oper = ""

    ostr = ""
    for tok in out:
        ostr += tok + " "
    uplog("upnpsearchtorecoll:out: <%s>" % ostr)
    return ostr


def search(rclconfdir, objid, upnps, idprefix, httphp, pathprefix):
    rcls = upnpsearchtorecoll(upnps)

    filterdir = uprclfolders.dirpath(objid)
    if filterdir and filterdir != "/":
        rcls += " dir:\"" + filterdir + "\""
    
    uplog("Search: recoll search: <%s>" % rcls)

    rcldb = recoll.connect(confdir=rclconfdir)
    try:
        rclq = rcldb.query()
        rclq.execute(rcls)
    except Exception as e:
        uplog("Search: recoll query raised: %s" % e)
        return []
    
    uplog("Estimated query results: %d" % (rclq.rowcount))
    if rclq.rowcount == 0:
        return []
    
    entries = []
    maxcnt = 0
    while True:
        docs = rclq.fetchmany()
        for doc in docs:
            id = idprefix + '$' + 'seeyoulater'
            e = rcldoctoentry(id, objid, httphp, pathprefix, doc)
            if e:
                entries.append(e)
        if (maxcnt > 0 and len(entries) >= maxcnt) or \
               len(docs) != rclq.arraysize:
            break
    uplog("Search retrieved %d docs" % (len(entries),))

    return sorted(entries, cmp=cmpentries)



if __name__ == '__main__':
    s = '(upnp:artist derivedFrom  "abc\\"def\\g") or (dc:title:xxx) '
    print("INPUT: %s" % s)
    o = upnpsearchtorecoll(s)
    print("OUTPUT: %s" % o)
