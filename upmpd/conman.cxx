/* Copyright (C) 2014 J.F.Dockes
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

#include "conman.hxx"

#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS, etc

#include <functional>                   // for _Bind, bind, _1, _2
#include <iostream>                     // for endl
#include <map>                          // for _Rb_tree_const_iterator, etc
#include <string>                       // for string, basic_string
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/log.hxx"             // for LOGDEB
#include "libupnpp/soaphelp.hxx"        // for SoapData, SoapArgs

#include "upmpd.hxx"                    // for UpMpd

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

static const string sTpCM("urn:schemas-upnp-org:service:ConnectionManager:1");
static const string sIdCM("urn:upnp-org:serviceId:ConnectionManager");

UpMpdConMan::UpMpdConMan(UpMpd *dev)
    : UpnpService(sTpCM, sIdCM, dev)
{
    dev->addActionMapping(this,"GetCurrentConnectionIDs", 
                          bind(&UpMpdConMan::getCurrentConnectionIDs, 
                               this, _1,_2));
    dev->addActionMapping(this,"GetCurrentConnectionInfo", 
                          bind(&UpMpdConMan::getCurrentConnectionInfo, 
                               this,_1,_2));
    dev->addActionMapping(this,"GetProtocolInfo", 
                          bind(&UpMpdConMan::getProtocolInfo, this, _1, _2));
}

bool UpMpdConMan::getEventData(bool all, std::vector<std::string>& names, 
                                 std::vector<std::string>& values)
{
    //LOGDEB("UpMpd:getEventDataCM" << endl);

    // Our data never changes, so if this is not an unconditional request,
    // we return nothing.
    if (all) {
        names.push_back("SinkProtocolInfo");
        values.push_back(upmpdProtocolInfo);
    }
    return true;
}

int UpMpdConMan::getCurrentConnectionIDs(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("UpMpd:getCurrentConnectionIDs" << endl);
    data.addarg("ConnectionIDs", "0");
    return UPNP_E_SUCCESS;
}

int UpMpdConMan::getCurrentConnectionInfo(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("UpMpdConMan:getCurrentConnectionInfo" << endl);
    map<string, string>::const_iterator it;
    it = sc.args.find("ConnectionID");
    if (it == sc.args.end() || it->second.empty()) {
        return UPNP_E_INVALID_PARAM;
    }
    if (it->second.compare("0")) {
        return UPNP_E_INVALID_PARAM;
    }

    data.addarg("RcsID", "0");
    data.addarg("AVTransportID", "0");
    data.addarg("ProtocolInfo", "");
    data.addarg("PeerConnectionManager", "");
    data.addarg("PeerConnectionID", "-1");
    data.addarg("Direction", "Input");
    data.addarg("Status", "Unknown");

    return UPNP_E_SUCCESS;
}

int UpMpdConMan::getProtocolInfo(const SoapArgs& sc, SoapData& data)
{
    LOGDEB("UpMpdConMan::getProtocolInfo" << endl);
    data.addarg("Source", "");
    data.addarg("Sink", upmpdProtocolInfo);

    return UPNP_E_SUCCESS;
}
