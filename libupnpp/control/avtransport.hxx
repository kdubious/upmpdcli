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
#ifndef _AVTRANSPORT_HXX_INCLUDED_
#define _AVTRANSPORT_HXX_INCLUDED_

#include <string>

#include "service.hxx"

namespace UPnPClient {

/**
 * AVTransport Service client class.
 *
 */
class AVTransport : public Service {
public:

    /** Construct by copying data from device and service objects.
     *
     */
    AVTransport(const UPnPDeviceDesc& device,
                const UPnPServiceDesc& service)
        : Service(device, service)
        {
            registerCallback();
        }

    AVTransport() {}

    /** Test service type from discovery message */
    static bool isAVTService(const std::string& st);


protected:
    static const string SType;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();

};

} // namespace UPnPClient

#endif /* _AVTRANSPORT_HXX_INCLUDED_ */
