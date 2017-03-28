#!/usr/bin/env python
# Copyright (C) 2016 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


from __future__ import print_function

import locale
import re
import os
import sys
import base64
import platform

class ConfSimple:
    """A ConfSimple class reads a recoll configuration file, which is a typical
    ini file (see the Recoll manual). It's a dictionary of dictionaries which
    lets you retrieve named values from the top level or a subsection"""

    def __init__(self, confname, tildexp = False, readonly = True):
        self.submaps = {}
        self.dotildexpand = tildexp
        self.readonly = readonly
        self.confname = confname
        
        try:
            f = open(confname, 'r')
        except Exception as exc:
            #print("Open Exception: %s" % exc, sys.stderr)
            # File does not exist -> empty config, not an error.
            return

        self.parseinput(f)
        
    def parseinput(self, f):
        appending = False
        line = ''
        submapkey = ''
        for cline in f:
            cline = cline.rstrip("\r\n")
            if appending:
                line = line + cline
            else:
                line = cline
            line = line.strip()
            if line == '' or line[0] == '#':
                continue

            if line[len(line)-1] == '\\':
                line = line[0:len(line)-1]
                appending = True
                continue
            appending = False
            #print(line)
            if line[0] == '[':
                line = line.strip('[]')
                if self.dotildexpand:
                    submapkey = os.path.expanduser(line)
                else:
                    submapkey = line
                #print("Submapkey: [%s]" % submapkey)
                continue
            nm, sep, value = line.partition('=')
            if sep == '':
                continue
            nm = nm.strip()
            value = value.strip()
            #print("Name: [%s] Value: [%s]" % (nm, value))

            if not submapkey in self.submaps:
                self.submaps[submapkey] = {}
            self.submaps[submapkey][nm] = value

    def get(self, nm, sk = ''):
        '''Returns None if not found, empty string if found empty'''
        if not sk in self.submaps:
            return None
        if not nm in self.submaps[sk]:
            return None
        return self.submaps[sk][nm]

    def _rewrite(self):
        if self.readonly:
            raise Exception("ConfSimple is readonly")

        tname = self.confname + "-"
        f = open(tname, 'w')
        # First output null subkey submap
        if '' in self.submaps:
            for nm,value in self.submaps[''].iteritems():
                f.write(nm + " = " + value + "\n")
        for sk,mp in self.submaps.iteritems():
            if sk == '':
                continue
            f.write("[" + sk + "]\n")
            for nm,value in mp.iteritems():
                f.write(nm + " = " + value + "\n")
        f.close()
        os.rename(tname, self.confname)

    def set(self, nm, value, sk = ''):
        if self.readonly:
            raise Exception("ConfSimple is readonly")
        self.submaps[sk][nm] = value
        self._rewrite()
        return True

    def getNames(self, sk = ''):
        if not sk in self.submaps:
            return None
        return list(self.submaps[sk].keys())
    
class ConfTree(ConfSimple):
    """A ConfTree adds path-hierarchical interpretation of the section keys,
    which should be '/'-separated values. When a value is requested for a
    given path, it will also be searched in the sections corresponding to
    the ancestors. E.g. get(name, '/a/b') will also look in sections '/a' and
    '/' or '' (the last 2 are equivalent)"""
    def get(self, nm, sk = ''):
        if sk == '' or sk[0] != '/':
            return ConfSimple.get(self, nm, sk)
            
        if sk[len(sk)-1] != '/':
            sk = sk + '/'

        # Try all sk ancestors as submaps (/a/b/c-> /a/b/c, /a/b, /a, '')
        while sk.find('/') != -1:
            val = ConfSimple.get(self, nm, sk)
            if val is not None:
                return val
            i = sk.rfind('/')
            if i == -1:
                break
            sk = sk[:i]

        return ConfSimple.get(self, nm)

class ConfStack:
    """ A ConfStack manages the superposition of a list of Configuration
    objects. Values are looked for in each object from the list until found.
    This typically provides for defaults overriden by sparse values in the
    topmost file."""

    def __init__(self, nm, dirs, tp = 'simple'):
        fnames = []
        for dir in dirs:
            fnm = os.path.join(dir, nm)
            fnames.append(fnm)
            self._construct(tp, fnames)

    def _construct(self, tp, fnames):
        self.confs = []
        for fname in fnames:
            if tp.lower() == 'simple':
                conf = ConfSimple(fname)
            else:
                conf = ConfTree(fname)
            self.confs.append(conf)

    def get(self, nm, sk = ''):
        for conf in self.confs:
            value = conf.get(nm, sk)
            if value is not None:
                return value
        return None

if __name__ == '__main__':
    def Usage():
        print("Usage: conftree.py <filename> <paramname> [<section>]",
              file=sys.stderr)
        sys.exit(1)
    section = ''
    if len(sys.argv) >= 3:
        fname = sys.argv[1]
        pname = sys.argv[2]
        if len(sys.argv) == 4:
            section = sys.argv[3]
        elif len(sys.argv) != 3:
            Usage()
    else:
        Usage()

    conf = ConfSimple(fname)
    if section:
        print("[%s] %s -> %s" % (section, pname, conf.get(pname)))
    else:
        print("%s -> %s" % (pname, conf.get(pname)))
