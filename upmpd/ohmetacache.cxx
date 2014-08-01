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
#include "libupnpp/workqueue.hxx"
#include "libupnpp/log.hxx"
#include "base64.hxx"

class SaveCacheTask {
public:
    SaveCacheTask(const string& fn, const mcache_type& cache)
        : m_fn(fn), m_cache(cache)
        {}

    string m_fn;
    mcache_type m_cache;
};
static WorkQueue<SaveCacheTask*> saveQueue("SaveQueue");

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

bool dmcacheSave(const string& fn, const mcache_type& cache)
{
    SaveCacheTask *tsk = new SaveCacheTask(fn, cache);

    // Use the flush option to put() so that only the latest version
    // stays on the queue, possibly saving writes.
    if (!saveQueue.put(tsk, true)) {
        LOGERR("dmcacheSave: can't queue save task" << endl);
        return false;
    }
    return true;
}

static void *dmcacheSaveWorker(void *)
{
    for (;;) {
        SaveCacheTask *tsk = 0;
        size_t qsz;
        if (!saveQueue.take(&tsk, &qsz)) {
            LOGERR("dmcacheSaveWorker: can't get task from queue" << endl);
            saveQueue.workerExit();
            return (void*)1;
        }
        LOGDEB("dmcacheSave: got save task: " << tsk->m_cache.size() << 
               " entries to " << tsk->m_fn << endl);

        // Beware that calling the constructor with a std::string
        // would result in a memory-based object...
        ConfSimple cs(tsk->m_fn.c_str());
        cs.clear();
        if (cs.getStatus() != ConfSimple::STATUS_RW) {
            LOGERR("dmcacheSave: could not open " << tsk->m_fn 
                   << " for writing" << endl);
            delete tsk;
            continue;
        }
        cs.holdWrites(true);
        for (mcache_type::const_iterator it = tsk->m_cache.begin();
             it != tsk->m_cache.end(); it++) {
            string name = base64_encode(it->first);
            eqtoggle(name);
            string value = base64_encode(it->second);
            //LOGDEB("dmcacheSave: " << name << " -> " << value << endl);
            cs.set(name, value);
        }
    
        if (!cs.holdWrites(false)) {
            LOGERR("dmcacheSave: write error while saving to " << 
                   tsk->m_fn << endl);
        }
        delete tsk;
    }
}

bool dmcacheRestore(const string& fn, mcache_type& cache)
{
    // Restore is called once at startup, so seize the opportunity to start the
    // save thread
    if (!saveQueue.start(1, dmcacheSaveWorker, 0)) {
        LOGERR("dmcacheRestore: could not start save thread" << endl);
        return false;
    }

    // Beware that calling the constructor with a std::string
    // would result in a memory-based object...
    ConfSimple cs(fn.c_str(), 1);
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

