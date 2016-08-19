/* Copyright (C) 2016 J.F.Dockes
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

#include "contentdirectory.hxx"

#include <upnp/upnp.h>

#include <functional>
#include <iostream>
#include <map>
#include <utility>

#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device/device.hxx"

using namespace std;
using namespace std::placeholders;

static const string
sTpContentDirectory("urn:schemas-upnp-org:service:ContentDirectory:1");
static const string
sIdContentDirectory("urn:upnp-org:serviceId:ContentDirectory");

ContentDirectory::ContentDirectory(UPnPProvider::UpnpDevice *dev)
    : UpnpService(sTpContentDirectory, sIdContentDirectory, dev)
{
    dev->addActionMapping(
        this, "GetSearchCapabilities",
        bind(&ContentDirectory::actGetSearchCapabilities, this, _1, _2));
    dev->addActionMapping(
        this, "GetSortCapabilities",
        bind(&ContentDirectory::actGetSortCapabilities, this, _1, _2));
    dev->addActionMapping(
        this, "GetSystemUpdateID",
        bind(&ContentDirectory::actGetSystemUpdateID, this, _1, _2));
    dev->addActionMapping(
        this, "Browse",
        bind(&ContentDirectory::actBrowse, this, _1, _2));
    dev->addActionMapping(
        this, "Search",
        bind(&ContentDirectory::actSearch, this, _1, _2));
}


int ContentDirectory::actGetSearchCapabilities(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("ContentDirectory::actGetSearchCapabilities: " << endl);

    std::string out_SearchCaps;
    data.addarg("SearchCaps", out_SearchCaps);
    return UPNP_E_SUCCESS;
}

int ContentDirectory::actGetSortCapabilities(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("ContentDirectory::actGetSortCapabilities: " << endl);

    std::string out_SortCaps;
    data.addarg("SortCaps", out_SortCaps);
    return UPNP_E_SUCCESS;
}

int ContentDirectory::actGetSystemUpdateID(const SoapIncoming& sc, SoapOutgoing& data)
{

    LOGDEB("ContentDirectory::actGetSystemUpdateID: " << endl);

    std::string out_Id;
    data.addarg("Id", out_Id);
    return UPNP_E_SUCCESS;
}

int ContentDirectory::actBrowse(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_ObjectID;
    ok = sc.get("ObjectID", &in_ObjectID);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no ObjectID in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_BrowseFlag;
    ok = sc.get("BrowseFlag", &in_BrowseFlag);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no BrowseFlag in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_Filter;
    ok = sc.get("Filter", &in_Filter);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no Filter in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_StartingIndex;
    ok = sc.get("StartingIndex", &in_StartingIndex);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no StartingIndex in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_RequestedCount;
    ok = sc.get("RequestedCount", &in_RequestedCount);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no RequestedCount in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SortCriteria;
    ok = sc.get("SortCriteria", &in_SortCriteria);
    if (!ok) {
        LOGERR("ContentDirectory::actBrowse: no SortCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("ContentDirectory::actBrowse: " << " ObjectID " << in_ObjectID << " BrowseFlag " << in_BrowseFlag << " Filter " << in_Filter << " StartingIndex " << in_StartingIndex << " RequestedCount " << in_RequestedCount << " SortCriteria " << in_SortCriteria << endl);

    std::string out_Result;
    std::string out_NumberReturned;
    std::string out_TotalMatches;
    std::string out_UpdateID;

    data.addarg("Result", out_Result);
    data.addarg("NumberReturned", out_NumberReturned);
    data.addarg("TotalMatches", out_TotalMatches);
    data.addarg("UpdateID", out_UpdateID);
    return UPNP_E_SUCCESS;
}

int ContentDirectory::actSearch(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_ContainerID;
    ok = sc.get("ContainerID", &in_ContainerID);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no ContainerID in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SearchCriteria;
    ok = sc.get("SearchCriteria", &in_SearchCriteria);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no SearchCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_Filter;
    ok = sc.get("Filter", &in_Filter);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no Filter in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_StartingIndex;
    ok = sc.get("StartingIndex", &in_StartingIndex);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no StartingIndex in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    int in_RequestedCount;
    ok = sc.get("RequestedCount", &in_RequestedCount);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no RequestedCount in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_SortCriteria;
    ok = sc.get("SortCriteria", &in_SortCriteria);
    if (!ok) {
        LOGERR("ContentDirectory::actSearch: no SortCriteria in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("ContentDirectory::actSearch: " << " ContainerID " << in_ContainerID << " SearchCriteria " << in_SearchCriteria << " Filter " << in_Filter << " StartingIndex " << in_StartingIndex << " RequestedCount " << in_RequestedCount << " SortCriteria " << in_SortCriteria << endl);

    std::string out_Result;
    std::string out_NumberReturned;
    std::string out_TotalMatches;
    std::string out_UpdateID;

    out_NumberReturned = "0";
    out_TotalMatches = "0";

    data.addarg("Result", out_Result);
    data.addarg("NumberReturned", out_NumberReturned);
    data.addarg("TotalMatches", out_TotalMatches);
    data.addarg("UpdateID", out_UpdateID);
    return UPNP_E_SUCCESS;
}


