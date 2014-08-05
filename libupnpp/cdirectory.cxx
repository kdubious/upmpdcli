/* Copyright (C) 2013 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <set>
#include <vector>
using namespace std;
#include <functional>
using namespace std::placeholders;

#include "upnpp_p.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include "upnpplib.hxx"
#include "ixmlwrap.hxx"
#include "cdirectory.hxx"
#include "cdircontent.hxx"
#include "discovery.hxx"

namespace UPnPClient {

// The service type string for Content Directories:
const string ContentDirectoryService::SType("urn:schemas-upnp-org:service:ContentDirectory:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool ContentDirectoryService::isCDService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}


static bool DSAccum(vector<ContentDirectoryService>* out,
                    const UPnPDeviceDesc& device, 
                    const UPnPServiceDesc& service)
{
    if (ContentDirectoryService::isCDService(service.serviceType)) {
        out->push_back(ContentDirectoryService(device, service));
    }
    return true;
}

bool ContentDirectoryService::getServices(vector<ContentDirectoryService>& vds)
{
    //LOGDEB("UPnPDeviceDirectory::getDirServices" << endl);
    UPnPDeviceDirectory::Visitor visitor = bind(DSAccum, &vds, _1, _2);
	UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    return !vds.empty();
}

static bool DSFriendlySelect(const string& friendlyName,
                             bool  *found,
                             ContentDirectoryService *out,
                             const UPnPDeviceDesc& device, 
                             const UPnPServiceDesc& service)
{
    if (ContentDirectoryService::isCDService(service.serviceType)) {
        if (!friendlyName.compare(device.friendlyName)) {
            *out = ContentDirectoryService(device, service);
            *found = true;
            return false;
        }
    }
    return true;
}

// Get server by friendly name. 
bool ContentDirectoryService::getServerByName(const string& friendlyName,
											  ContentDirectoryService& server)
{
    bool found = false;
    UPnPDeviceDirectory::Visitor visitor = 
        bind(DSFriendlySelect, friendlyName, &found, &server, _1, _2);
	UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    return found;
}


class DirBResFree {
public:
    IXML_Document **rqpp, **rspp;
    DirBResFree(IXML_Document** _rqpp, IXML_Document **_rspp)
        :rqpp(_rqpp), rspp(_rspp)
        {}
    ~DirBResFree()
        {
            if (*rqpp)
                ixmlDocument_free(*rqpp);
            if (*rspp)
                ixmlDocument_free(*rspp);
        }
};

int ContentDirectoryService::readDirSlice(const string& objectId, int offset,
                                          int count, UPnPDirContent& dirbuf,
                                          int *didreadp, int *totalp)
{
    PLOGDEB("CDService::readDirSlice: objId [%s] offset %d count %d\n",
            objectId.c_str(), offset, count);
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        PLOGINF("CDService::readDir: no lib\n");
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    IXML_Document *request(0);
    IXML_Document *response(0);
    DirBResFree cleaner(&request, &response);

    // Create request
    char ofbuf[100], cntbuf[100];
    sprintf(ofbuf, "%d", offset);
    sprintf(cntbuf, "%d", count);
    int argcnt = 6;
    // Some devices require an empty SortCriteria, else bad params
    request = UpnpMakeAction("Browse", m_serviceType.c_str(), argcnt,
                             "ObjectID", objectId.c_str(),
                             "BrowseFlag", "BrowseDirectChildren",
                             "Filter", "*",
                             "SortCriteria", "",
                             "StartingIndex", ofbuf,
                             "RequestedCount", cntbuf,
                             NULL, NULL);
    if (request == 0) {
        PLOGINF("CDService::readDir: UpnpMakeAction failed\n");
        return  UPNP_E_OUTOF_MEMORY;
    }

    //cerr << "Action xml: [" << ixmlPrintDocument(request) << "]" << endl;

    int ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                             0 /*devUDN*/, request, &response);

    if (ret != UPNP_E_SUCCESS) {
        PLOGINF("CDService::readDir: UpnpSendAction failed: %s\n",
                UpnpGetErrorMessage(ret));
        return ret;
    }

    int didread = -1;
    string tbuf = ixmlwrap::getFirstElementValue(response, "NumberReturned");
    if (!tbuf.empty())
        didread = atoi(tbuf.c_str());

    if (count == -1 || count == 0) {
        PLOGINF("CDService::readDir: got -1 or 0 entries\n");
        return UPNP_E_BAD_RESPONSE;
    }

    tbuf = ixmlwrap::getFirstElementValue(response, "TotalMatches");
    if (!tbuf.empty())
        *totalp = atoi(tbuf.c_str());

    tbuf = ixmlwrap::getFirstElementValue(response, "Result");

#if 0
    cerr << "CDService::readDirSlice: count " << count <<
        " offset " << offset <<
        " total " << *totalp << endl;
    cerr << " result " << tbuf << endl;
#endif

    dirbuf.parse(tbuf);
    *didreadp = didread;
    return UPNP_E_SUCCESS;
}


int ContentDirectoryService::readDir(const string& objectId,
                                     UPnPDirContent& dirbuf)
{
    PLOGDEB("CDService::readDir: url [%s] type [%s] udn [%s] objId [%s]\n",
            m_actionURL.c_str(), m_serviceType.c_str(), m_deviceId.c_str(),
            objectId.c_str());

    int offset = 0;
    int total = 1000;// Updated on first read.

    while (offset < total) {
        int count;
        int error = readDirSlice(objectId, offset, m_rdreqcnt, dirbuf,
                                 &count, &total);
        if (error != UPNP_E_SUCCESS)
            return error;

        offset += count;
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectoryService::search(const string& objectId,
                                    const string& ss,
                                    UPnPDirContent& dirbuf)
{
    PLOGDEB("CDService::search: url [%s] type [%s] udn [%s] objid [%s] "
            "search [%s]\n",
            m_actionURL.c_str(), m_serviceType.c_str(), m_deviceId.c_str(),
            objectId.c_str(), ss.c_str());

    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        PLOGINF("CDService::search: no lib\n");
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    int ret = UPNP_E_SUCCESS;
    IXML_Document *request(0);
    IXML_Document *response(0);

    int offset = 0;
    int total = 1000;// Updated on first read.

    while (offset < total) {
        DirBResFree cleaner(&request, &response);
        char ofbuf[100];
        sprintf(ofbuf, "%d", offset);
        // Create request
        int argcnt = 6;
        request = UpnpMakeAction(
            "Search", m_serviceType.c_str(), argcnt,
            "ContainerID", objectId.c_str(),
            "SearchCriteria", ss.c_str(),
            "Filter", "*",
            "SortCriteria", "",
            "StartingIndex", ofbuf,
            "RequestedCount", "0", // Setting a value here gets twonky into fits
            NULL, NULL);
        if (request == 0) {
            PLOGINF("CDService::search: UpnpMakeAction failed\n");
            return  UPNP_E_OUTOF_MEMORY;
        }

        // cerr << "Action xml: [" << ixmlPrintDocument(request) << "]" << endl;

        ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                             0 /*devUDN*/, request, &response);

        if (ret != UPNP_E_SUCCESS) {
            PLOGINF("CDService::search: UpnpSendAction failed: %s\n",
                    UpnpGetErrorMessage(ret));
            return ret;
        }

        int count = -1;
        string tbuf =
            ixmlwrap::getFirstElementValue(response, "NumberReturned");
        if (!tbuf.empty())
            count = atoi(tbuf.c_str());

        if (count == -1 || count == 0) {
            PLOGINF("CDService::search: got -1 or 0 entries\n");
            return count == -1 ? UPNP_E_BAD_RESPONSE : UPNP_E_SUCCESS;
        }
        offset += count;

        tbuf = ixmlwrap::getFirstElementValue(response, "TotalMatches");
        if (!tbuf.empty())
            total = atoi(tbuf.c_str());

        tbuf = ixmlwrap::getFirstElementValue(response, "Result");

#if 0
        cerr << "CDService::search: count " << count <<
            " offset " << offset <<
            " total " << total << endl;
        cerr << " result " << tbuf << endl;
#endif

        dirbuf.parse(tbuf);
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectoryService::getSearchCapabilities(set<string>& result)
{
    PLOGDEB("CDService::getSearchCapabilities:\n");
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        PLOGINF("CDService::getSearchCapabilities: no lib\n");
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    int ret = UPNP_E_SUCCESS;
    IXML_Document *request(0);
    IXML_Document *response(0);

    request = UpnpMakeAction("GetSearchCapabilities", m_serviceType.c_str(),
                             0,
                             NULL, NULL);

    if (request == 0) {
        PLOGINF("CDService::getSearchCapa: UpnpMakeAction failed\n");
        return  UPNP_E_OUTOF_MEMORY;
    }

    //cerr << "Action xml: [" << ixmlPrintDocument(request) << "]" << endl;

    ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                         0 /*devUDN*/, request, &response);

    if (ret != UPNP_E_SUCCESS) {
        PLOGINF("CDService::getSearchCapa: UpnpSendAction failed: %s\n",
                UpnpGetErrorMessage(ret));
        return ret;
    }
    //cerr << "getSearchCapa: response xml: [" << ixmlPrintDocument(response)
    // << "]" << endl;

    string tbuf = ixmlwrap::getFirstElementValue(response, "SearchCaps");
    // cerr << "getSearchCapa: capa: [" << tbuf << "]" << endl;

    result.clear();
    if (!tbuf.compare("*")) {
        result.insert(result.end(), "*");
    } else if (!tbuf.empty()) {
        if (!csvToStrings(tbuf, result)) {
            return UPNP_E_BAD_RESPONSE;
        }
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectoryService::getMetadata(const string& objectId,
                                         UPnPDirContent& dirbuf)
{
    PLOGDEB("CDService::getMetadata: url [%s] type [%s] udn [%s] objId [%s]\n",
            m_actionURL.c_str(), m_serviceType.c_str(), m_deviceId.c_str(),
            objectId.c_str());

    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        PLOGINF("CDService::getMetadata: no lib\n");
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    int ret = UPNP_E_SUCCESS;
    IXML_Document *request(0);
    IXML_Document *response(0);

    DirBResFree cleaner(&request, &response);
    // Create request
    int argcnt = 6;
    request = UpnpMakeAction("Browse", m_serviceType.c_str(), argcnt,
                             "ObjectID", objectId.c_str(),
                             "BrowseFlag", "BrowseMetadata",
                             "Filter", "*",
                             "SortCriteria", "",
                             "StartingIndex", "0",
                             "RequestedCount", "1",
                             NULL, NULL);
    if (request == 0) {
        PLOGINF("CDService::getmetadata: UpnpMakeAction failed\n");
        return  UPNP_E_OUTOF_MEMORY;
    }

    //cerr << "Action xml: [" << ixmlPrintDocument(request) << "]" << endl;

    ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                         0 /*devUDN*/, request, &response);

    if (ret != UPNP_E_SUCCESS) {
        PLOGINF("CDService::getmetadata: UpnpSendAction failed: %s\n",
                UpnpGetErrorMessage(ret));
        return ret;
    }
    string tbuf = ixmlwrap::getFirstElementValue(response, "Result");
    if (dirbuf.parse(tbuf))
        return UPNP_E_SUCCESS;
    else
        return UPNP_E_BAD_RESPONSE;
}

} // namespace UPnPClient
