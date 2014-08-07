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
#ifndef _RENDERINGCONTROL_HXX_INCLUDED_
#define _RENDERINGCONTROL_HXX_INCLUDED_

#include <string>

#include "service.hxx"

namespace UPnPClient {

/**
 * RenderingControl Service client class.
 *
 */
class RenderingControl : public Service {
public:

    /** Construct by copying data from device and service objects.
     *
     */
    RenderingControl(const UPnPDeviceDesc& device,
                            const UPnPServiceDesc& service)
        : Service(device, service)
        {}

    RenderingControl() {}

    /** Test service type from discovery message */
    static bool isRDCService(const std::string& st);

    /** @ret 0 for success, upnp error else */
    int setVolume(int volume, const string& channel = "Master");
    int getVolume(const string& channel = "Master");

protected:
    static const string SType;
};

} // namespace UPnPClient

#endif /* _RENDERINGCONTROL_HXX_INCLUDED_ */
