/* Copyright (C) 2014 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <unistd.h>
#include <fcntl.h>

#include <iostream>
using namespace std;

#include "ohmetacache.hxx"
#include "conftree.hxx"
#include "libupnpp/log.hxx"
#include "base64.hxx"

// We use base64 to encode names and value, and need to replace the '='
static const char eqesc = '*';
static void eqtoggle(string& nm)
{
    int i = nm.size() - 1;
    while (i >= 0) {
        if (nm[i] == '=') {
            nm[i] = eqesc;
        } else if (nm[i] == eqesc) {
            nm[i] = '=';
        } else {
            break;
        }
        i--;
    }
}

bool dmcacheSave(const char *fn, const mcache_type& cache)
{
    ConfSimple cs(fn);
    if (cs.getStatus() != ConfSimple::STATUS_RW) {
        LOGERR("dmcacheSave: could not open " << fn << " for writing" << endl);
        return false;
    }
    cs.clear();
    cs.holdWrites(true);
    for (mcache_type::const_iterator it = cache.begin();
         it != cache.end(); it++) {
        //LOGDEB("dmcacheSave: " << it->first << " -> " << it->second << endl);
        string name = base64_encode(it->first);
        eqtoggle(name);
        cs.set(name, base64_encode(it->second));
    }
    
    if (!cs.holdWrites(false)) {
        LOGERR("dmcacheSave: write error while saving to " << fn << endl);
        return false;
    }
    return true;
}

bool dmcacheRestore(const char *fn, mcache_type& cache)
{
    ConfSimple cs(fn, 1);
    if (!cs.ok()) {
        LOGINF("dmcacheRestore: could not read metadata from " << fn << endl);
        return false;
    }

    vector<string> names = cs.getNames("");
    for (auto &name : names) {
        string value;
        if (!cs.get(name, value)) {
            // ??
            LOGDEB("dmcacheRestore: no data for found name " << name << endl);
            continue;
        }
        eqtoggle(name);
        name = base64_decode(name); 
        value = base64_decode(value);
        LOGDEB("dmcacheRestore: " << name << " -> " << 
               value.substr(0, 20) << endl);
        cache[name] = value;
    }
    return true;
}

