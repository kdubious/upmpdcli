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
#ifndef _MEDIARENDERER_HXX_INCLUDED_
#define _MEDIARENDERER_HXX_INCLUDED_

#include "libupnpp/description.hxx"

namespace UPnPClient {

class Device {
public:
    /* Something may get there one day... */
};

class MediaRenderer : public Device {
public:
    static bool getDeviceDescs(std::vector<UPnPDeviceDesc>& devices);
    static bool hasOpenHome(const UPnPDeviceDesc& device);
    static bool isMRDevice(const string& devicetype);

protected:
    static const std::string DType;
};

}

#endif /* _MEDIARENDERER_HXX_INCLUDED_ */
