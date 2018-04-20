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

import sys
import os
import shutil
import conftree
import subprocess
import time
import copy

from upmplgutils import uplog

def _initconfdir(confdir, topdirs):
    if os.path.exists(confdir):
        raise Exception("_initconfdir: exists already: %s" % confdir)
    os.mkdir(confdir)
    datadir = os.path.dirname(__file__)
    uplog("datadir: %s" % datadir)
    shutil.copyfile(os.path.join(datadir, "rclconfig-fields"),
                    os.path.join(confdir, "fields"))
    f = open(os.path.join(confdir, "recoll.conf"), "w")
    f.write("topdirs=%s\n" % topdirs)
    f.write("idxabsmlen=0\n")
    f.write("loglevel=2\n")
    f.write("noaspell=1\n")
    f.write("testmodifusemtime=1\n")
    f.close()

    # Only very recent Recoll versions have support for wavpack (only
    # config data is missing, rclaudio processes the files all
    # right). Make sure that we have what's needed.
    f = open(os.path.join(confdir, "mimemap"), "w")
    f.write(".ape = audio/ape\n")
    f.write(".mpc = audio/x-musepack\n")
    f.write(".wv = audio/x-wavpack\n")
    f.close()
    f = open(os.path.join(confdir, "mimeconf"), "w")
    f.write("[index]\n")
    f.write("audio/ape = execm rclaudio\n")
    f.write("audio/x-musepack = execm rclaudio\n")
    f.write("audio/x-wavpack = execm rclaudio\n")
    f.close()

_idxproc = None
_lastidxstatus = None

def runindexer(confdir, topdirs):
    global _idxproc, _lastidxstatus
    if _idxproc is not None:
        raise Exception("uprclrunindexer: already running")

    if not os.path.isdir(confdir):
        if os.path.exists(confdir):
            raise Exception("Exists and not directory: %s" % confdir)
        _initconfdir(confdir, topdirs)
    else:
        cf = conftree.ConfSimple(os.path.join(confdir, "recoll.conf"),
                                 readonly = False)
        td = cf.get("topdirs", '')
        if td != topdirs:
            cf.set("topdirs", topdirs)

    env = copy.deepcopy(os.environ)
    env["HOME"] = confdir
    _idxproc = subprocess.Popen(["recollindex", "-c", confdir])

def indexerdone():
    global _idxproc, _lastidxstatus
    if _idxproc is None:
        return True
    _lastidxstatus = _idxproc.poll()
    if _lastidxstatus is None:
        return False
    _idxproc = None
    return True

def indexerstatus():
    return _lastidxstatus
    


# Only used for testing
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: uprclindex.py <confdir> <topdirs>", file=sys.stderr)
        sys.exit(1)
    runindexer(sys.argv[1], sys.argv[2])
    while True:
        if indexerdone():
            uplog("Indexing done, status: %d" % indexerstatus())
            sys.exit(0)
        uplog("Waiting for indexer")
        time.sleep(1);
