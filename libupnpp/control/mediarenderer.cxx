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

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
using namespace std;
using namespace std::placeholders;

#include "libupnpp/log.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/mediarenderer.hxx"
#include "libupnpp/control/renderingcontrol.hxx"

namespace UPnPClient {

const string 
MediaRenderer::DType("urn:schemas-upnp-org:device:MediaRenderer:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool MediaRenderer::isMRDevice(const string& st)
{
    const string::size_type sz(DType.size()-2);
    return !DType.compare(0, sz, st, 0, sz);
}

static bool MDAccum(unordered_map<string, UPnPDeviceDesc>* out,
                    const string& friendlyName,
                    const UPnPDeviceDesc& device, 
                    const UPnPServiceDesc& service)
{
    //LOGDEB("MDAccum: friendlyname: " << friendlyName << 
    //    " dev friendlyName " << device.friendlyName << endl);
    if (RenderingControl::isRDCService(service.serviceType) &&
        (friendlyName.empty() ? true : 
         !friendlyName.compare(device.friendlyName))) {
        //LOGDEB("MDAccum setting " << device.UDN << endl);
        (*out)[device.UDN] = device;
    }
    return true;
}

bool MediaRenderer::getDeviceDescs(vector<UPnPDeviceDesc>& devices, 
                                   const string& friendlyName)
{
    unordered_map<string, UPnPDeviceDesc> mydevs;

    UPnPDeviceDirectory::Visitor visitor = bind(MDAccum, &mydevs, friendlyName,
                                                _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    for (auto& entry : mydevs)
        devices.push_back(entry.second);
    return !devices.empty();
}

MediaRenderer::MediaRenderer(const UPnPDeviceDesc& desc)
{
    bool found = false;
    for (auto& entry : desc.services) {
        if (RenderingControl::isRDCService(entry.serviceType)) {
            m_rdc = RDCH(new RenderingControl(desc, entry));
            found = true;
            break;
        }
    }
    if (!found) {
        LOGERR("MediaRenderer::MediaRenderer: RenderingControl service not "
               "found in device" << endl);
    }

    found = false;
    for (auto& entry : desc.services) {
        if (AVTransport::isAVTService(entry.serviceType)) {
            m_avt = AVTH(new AVTransport(desc, entry));
            found = true;
            break;
        }
    }
    if (!found) {
        LOGERR("MediaRenderer::MediaRenderer: AVTransport service not "
               "found in device" << endl);
    }

}

bool MediaRenderer::hasOpenHome(const UPnPDeviceDesc& device)
{
    return false;
}

}

