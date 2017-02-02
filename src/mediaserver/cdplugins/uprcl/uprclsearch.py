#!/usr/bin/env python
from __future__ import print_function

import uprclfolders
from uprclutils import *
from recoll import recoll

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

# Called with '"' already read:
def _readstring(s, i):
    str = '"'
    escape = False
    for j in range(i, len(s)):
        #print("s[j] [%s] out now [%s]" % (s[j],out))
        if s[j] == '\\':
            if not escape:
                escape = True
                str += '\\'
            continue

        if s[j] == '"':
            str += '"'
            if not escape:
                return j+1, str
        else:
            str += s[j]
        
        escape = False        

    return len(s), str

def upnpsearchtorecoll(s):
    uplog("upnpsearchtorecoll:in: <%s>" % s)
    s = s.replace('\t', ' ')
    s = s.replace('\n', ' ')
    s = s.replace('\r', ' ')
    s = s.replace('\f', ' ')

    out = []
    hadDerived = False
    i = 0
    while True:
        i,c = _getchar(s, i)
        if not c:
            break

        if c.isspace():
            continue

        if c == "*":
            if (len(out) > 1 or (len(out) == 1 and not out[-1].isspace())) or \
                   (len(s[i:]) and not s[i:].isspace()):
                raise Exception("If * is used it must be the only input")
            out = ["mime:*"]
            break

        if c == '(' or c == ')' or c == '>' or c == '<' or c == '=':
            out.append(c)
        else:
            if c == '"':
                i,w = _readstring(s, i)
                if not w.endswith('"'):
                    raise Exception("Unterminated string in [%s]" % out)
            else:
                i -= 1
                i,w = _readword(s, i)

            #print("Got word [%s]" % w)
            if w == 'contains':
                out.append(':')
            elif w == 'doesNotContain':
                if len(out) < 1:
                    raise Exception("doesNotContain can't be the first word")
                out.insert(-1, "-")
                out.append(':')
            elif w == 'derivedFrom':
                hadDerived = True
                out.append(':')
            elif w == 'true':
                out.append('*')
            elif w == 'false':
                out.append('xxxjanzocsduochterrrrm')
            elif w == 'exists':
                out.append(':')
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
                if hadDerived:
                    hadDerived = False
                    if len(w) >= 1 and w[-1] == '"':
                        w = w[:-1] + '*' + '"'
                    else:
                        w += '*'
                out.append(w)

    ostr = ""
    for tok in out:
        ostr += tok + " "
    uplog("upnpsearchtorecoll:out: <%s>" % ostr)
    return ostr


def search(rclconfdir, objid, upnps, idprefix, httphp, pathprefix):
    rcls = upnpsearchtorecoll(upnps)

    filterdir = uprclfolders.dirpath(objid)
    rcls += " dir:\"" + filterdir + "\""
    
    uplog("Search: recoll search: %s" % rcls)

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
    totcnt = 0
    while True:
        docs = rclq.fetchmany()
        for doc in docs:
            id = idprefix + '$' + 'seeyoulater'
            e = rcldoctoentry(id, objid, httphp, pathprefix, doc)
            entries.append(e)
            totcnt += 1
        if (maxcnt > 0 and totcnt >= maxcnt) or len(docs) != rclq.arraysize:
            break
    uplog("Search retrieved %d docs" % (totcnt,))

    return entries



if __name__ == '__main__':
    s = '(upnp:artist derivedFrom  "abc\\"def\\g") or (dc:title:xxx) '
    print("INPUT: %s" % s)
    o = upnpsearchtorecoll(s)
    print("OUTPUT: %s" % o)
