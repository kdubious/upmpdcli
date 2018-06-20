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

#ifndef _STREAMPROXY_H_INCLUDED_
#define _STREAMPROXY_H_INCLUDED_

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

/// HTTP proxy for UPnP audio transfers
///
/// Uses microhttpd for the server part and libcurl for talking to the
/// real server.
///
class StreamProxy {
public:
    enum UrlTransReturn {Error, Proxy, Redirect};
    typedef std::function<UrlTransReturn
                          (std::string& url,
                           const std::unordered_map<std::string, std::string>&)>
    UrlTransFunc;

    StreamProxy(int listenport, UrlTransFunc urltrans);
    ~StreamProxy();

    void abortAll();

    class Internal;
private:
    std::unique_ptr<Internal> m;
};

#endif /* _STREAMPROXY_H_INCLUDED_ */
