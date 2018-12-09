/* Copyright (C) 2017-2018 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "netfetch.h"

#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std;

#ifndef MAX
#define MAX(A,B) ((A) < (B) ? (B) : (A))
#endif

size_t NetFetch::databufToQ(const void *contents, size_t bcnt)
{
    LOGDEB1("NetFetch::dataBufToQ. bcnt " << bcnt << endl);
    
    ABuffer *buf = nullptr;
    // Try to recover an empty buffer from the queue, else allocate one.
    if (outqueue && outqueue->take_recycled(&buf)) {
        if (buf->allocbytes < bcnt) {
            delete buf;
            buf = nullptr;
        }
    }
    if (buf == nullptr) {
        buf = new ABuffer(MAX(4096, bcnt));
    }
    if (buf == nullptr) {
        LOGERR("CurlFetch::dataBufToQ: can't get buffer for " << bcnt <<
               " bytes\n");
        return 0;
    }
    memcpy(buf->buf, contents, bcnt);
    buf->bytes = bcnt;
    buf->curoffs = 0;

    LOGDEB1("NetFetch::calling put on " <<
            (outqueue ? outqueue->getname() : "null") << endl);
    
    if (!outqueue->put(buf)) {
        LOGDEB1("CurlFetch::dataBufToQ. queue put failed\n");
        delete buf;
        return -1;
    }

    fetch_data_count += bcnt;
    if (fbcb) {
        fbcb(fetch_data_count);
    }
    LOGDEB1("CurlFetch::dataBufToQ. returning " << bcnt << endl);
    return bcnt;
}
