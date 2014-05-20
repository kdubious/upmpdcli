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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <set>
using namespace std;
using namespace std::placeholders;

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device.hxx"
#include "libupnpp/log.hxx"

#include "upmpd.hxx"
#include "conman.hxx"
#include "mpdcli.hxx"
#include "upmpdutils.hxx"

static const string serviceIdCM("urn:upnp-org:serviceId:ConnectionManager");

UpMpdConMan::UpMpdConMan(UpMpd *dev)
    : UpnpService(dev)
{
    dev->addService(this, serviceIdCM);
    dev->addActionMapping("GetCurrentConnectionIDs", 
                          bind(&UpMpdConMan::getCurrentConnectionIDs, 
                               this, _1,_2));
    dev->addActionMapping("GetCurrentConnectionInfo", 
                          bind(&UpMpdConMan::getCurrentConnectionInfo, 
                               this,_1,_2));
    dev->addActionMapping("GetProtocolInfo", 
                          bind(&UpMpdConMan::getProtocolInfo, this, _1, _2));
}

const std::string& UpMpdConMan::getServiceType()
{
    return serviceIdCM;
}

// "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3,"
// "http-get:*:audio/L16:DLNA.ORG_PN=LPCM,"
// "http-get:*:audio/x-flac:DLNA.ORG_PN=FLAC"
static const string 
myProtocolInfo(
    "http-get:*:audio/wav:*,"
    "http-get:*:audio/wave:*,"
    "http-get:*:audio/x-wav:*,"
    "http-get:*:audio/x-aiff:*,"
    "http-get:*:audio/mpeg:*,"
    "http-get:*:audio/x-mpeg:*,"
    "http-get:*:audio/mp1:*,"
    "http-get:*:audio/aac:*,"
    "http-get:*:audio/flac:*,"
    "http-get:*:audio/x-flac:*,"
    "http-get:*:audio/m4a:*,"
    "http-get:*:audio/mp4:*,"
    "http-get:*:audio/x-m4a:*,"
    "http-get:*:audio/vorbis:*,"
    "http-get:*:audio/ogg:*,"
    "http-get:*:audio/x-ogg:*,"
    "http-get:*:audio/x-scpls:*,"
    "http-get:*:audio/L16;rate=11025;channels=1:*,"
    "http-get:*:audio/L16;rate=22050;channels=1:*,"
    "http-get:*:audio/L16;rate=44100;channels=1:*,"
    "http-get:*:audio/L16;rate=48000;channels=1:*,"
    "http-get:*:audio/L16;rate=88200;channels=1:*,"
    "http-get:*:audio/L16;rate=96000;channels=1:*,"
    "http-get:*:audio/L16;rate=176400;channels=1:*,"
    "http-get:*:audio/L16;rate=192000;channels=1:*,"
    "http-get:*:audio/L16;rate=11025;channels=2:*,"
    "http-get:*:audio/L16;rate=22050;channels=2:*,"
    "http-get:*:audio/L16;rate=44100;channels=2:*,"
    "http-get:*:audio/L16;rate=48000;channels=2:*,"
    "http-get:*:audio/L16;rate=88200;channels=2:*,"
    "http-get:*:audio/L16;rate=96000;channels=2:*,"
    "http-get:*:audio/L16;rate=176400;channels=2:*,"
    "http-get:*:audio/L16;rate=192000;channels=2:*"
    );

bool UpMpdConMan::getEventData(bool all, std::vector<std::string>& names, 
                                 std::vector<std::string>& values)
{
    //LOGDEB("UpMpd:getEventDataCM" << endl);

    // Our data never changes, so if this is not an unconditional request,
    // we return nothing.
    if (all) {
        names.push_back("SinkProtocolInfo");
        values.push_back(myProtocolInfo);
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
    data.addarg("Sink", myProtocolInfo);

    return UPNP_E_SUCCESS;
}
