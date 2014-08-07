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
using namespace std;

#include "libupnpp/log.hxx"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/control/cdirectory.hxx"

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

// A small helper class for the functions which perform
// UpnpSendAction calls: get rid of IXML docs when done.
class IxmlCleaner {
public:
    IXML_Document **rqpp, **rspp;
    IxmlCleaner(IXML_Document** _rqpp, IXML_Document **_rspp)
        : rqpp(_rqpp), rspp(_rspp) {}
    ~IxmlCleaner()
     {
         if (*rqpp) ixmlDocument_free(*rqpp);
         if (*rspp) ixmlDocument_free(*rspp);
     }
};

int Service::runAction(const SoapEncodeInput& args, SoapDecodeOutput& data)
{
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::runAction: no lib" << endl);
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    IXML_Document *request(0);
    IXML_Document *response(0);
    IxmlCleaner cleaner(&request, &response);

    if ((request = buildSoapBody(args, false)) == 0) {
        LOGINF("Service::runAction: buildSoapBody failed" << endl);
        return  UPNP_E_OUTOF_MEMORY;
    }

    LOGDEB("Action xml: [" << ixmlPrintDocument(request) << "]" << endl);

    int ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                             0 /*devUDN*/, request, &response);

    if (ret != UPNP_E_SUCCESS) {
        LOGINF("Service::runAction: UpnpSendAction failed: " <<
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }
    LOGDEB("Result xml: [" << ixmlPrintDocument(response) << "]" << endl);

    if (!decodeSoapBody(args.name.c_str(), response, &data)) {
        LOGERR("Service::runAction: Could not decode response: " <<
               ixmlPrintDocument(response) << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return UPNP_E_SUCCESS;
}

}
