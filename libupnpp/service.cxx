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

#include "service.h"
#include "cdirectory.h"
#include "log.h"

namespace UPnPClient {

Service *service_factory(const string& servicetype,
                         const UPnPDeviceDesc& device,
                         const UPnPServiceDesc& service)
{
    if (ContentDirectoryService::isCDService(servicetype)) {
        return new ContentDirectoryService(device, service);
    } else {
        LOGERR("service_factory: unknown service type " << servicetype << endl);
        return 0;
    }
}

}
