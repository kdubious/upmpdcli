/* Copyright (C) 2017-2018 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _CURLFETCH_H_INCLUDED_
#define _CURLFETCH_H_INCLUDED_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <functional>
#include <memory>

#include "bufxchange.h"
#include "abuffer.h"
#include "netfetch.h"

//
// Wrapper for a libcurl transfer. This uses the curl_easy interface
// in a separate thread, so any number of transfers may be performed
// in parallel.
//
// The transfer is aborted when the object is deleted, with proper
// cleanup (in practise, if the curl thread is currently working
// inside curl_easy_transfer, the curl fd is closed to abort any
// current waiting).
//
// The end of transfer is signalled by pushing an empty buffer on the queue
//
// All methods are supposedly thread-safe
class CurlFetch : public NetFetch {
public:
    CurlFetch(const std::string& url);
    ~CurlFetch();

    const std::string& url();
    
    void setTimeout(int secs);
    
    /// Start the transfer to the output queue.
    bool start(BufXChange<ABuffer*> *queue, uint64_t offset = 0);

    // Wait for HTTP headers. This allows, e.g. doing stuff depending
    // on content-type before proceeding with the actual data transfer
    bool waitForHeaders(int maxSecs = 0);
    // Retrieve header value (after a successful waitForHeaders).
    bool headerValue(const std::string& nm, std::string& val);

    // Check if the curl thread is done and retrieve the results if it
    // is. This does not wait, it returns false if the transfer is
    // still running.
    bool fetchDone(FetchStatus *code, int *http_code) override;

    /// Reset after transfer done, for retrying for exemple.
    void reset();

    // Callbacks

    // A function to create the first buffer (typically for prepending
    // a wav header to a raw pcm stream. If set this is called from
    // the first curl write callback, before processing the curl data,
    // so this happens at a point where the client may have had a look
    // at the headers).
    void setBuf1GenCB(std::function<bool(std::string& buf,void*,int)>);
    // Called after curl_easy_perform returns
    void setEOFetchCB(std::function<void(bool ok, u_int64_t count)> eofcb);
    // Called every time we get new data from curl
    void setFetchBytesCB(std::function<void(u_int64_t count)> fbcb);
    
    class Internal;
private:
    std::unique_ptr<Internal> m;
};

#endif /* _CURLFETCH_H_INCLUDED_ */
