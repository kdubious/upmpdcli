#ifndef _CURLFETCH_H_INCLUDED_
#define _CURLFETCH_H_INCLUDED_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <functional>
#include <memory>

#include "bufxchange.h"
#include "abuffer.h"

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
class CurlFetch {
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
    bool curlDone(int *curlcode, int *http_code);

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
