/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _OHPLAYLIST_HXX_INCLUDED_
#define _OHPLAYLIST_HXX_INCLUDED_

#include <string>
#include <memory>

#include "service.hxx"

namespace UPnPClient {

class OHPlaylist;
typedef std::shared_ptr<OHPlaylist> OHPLH;

/**
 * OHPlaylist Service client class.
 *
 */
class OHPlaylist : public Service {
public:

    OHPlaylist(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service, false) {
    }

    OHPlaylist() {}

    /** Test service type from discovery message */
    static bool isOHPlService(const std::string& st);

protected:
    /* My service type string */
    static const std::string SType;
};

} // namespace UPnPClient

#endif /* _OHPLAYLIST_HXX_INCLUDED_ */
