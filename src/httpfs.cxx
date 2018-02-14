/* Copyright (C) 2014 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <unordered_map>
#include <string>

#include "libupnpp/log.hxx"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/device/device.hxx"

#include "main.hxx"
#include "upmpdutils.hxx"
#include "smallut.h"
#include "pathut.h"
#include "readfile.h"
#include "httpfs.hxx"
#include "conftree.h"

using namespace std;
using namespace UPnPP;
using namespace UPnPProvider;

// UPnP AV services. We can disable this to help pure OpenHome
// renderers which having both UPnP AV and OpenHome gets in trouble
// (Kinsky)
static string upnpAVDesc(
    "<service>"
    "  <serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>"
    "  <serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>"
    "  <SCPDURL>/upmpd/RenderingControl.xml</SCPDURL>"
    "  <controlURL>/ctl/RenderingControl</controlURL>"
    "  <eventSubURL>/evt/RenderingControl</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
    "  <serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>"
    "  <SCPDURL>/upmpd/AVTransport.xml</SCPDURL>"
    "  <controlURL>/ctl/AVTransport</controlURL>"
    "  <eventSubURL>/evt/AVTransport</eventSubURL>"
    "</service>"
    );

// The description XML document is the first thing downloaded by
// clients and tells them what services we export, and where to find
// them. The base data is in /usr/shared/upmpdcli/description.xml, it
// has a number of substitutable fields for optional data, which we
// conditionally substitute into the file data (e.g. description of
// OpenHome or other optional services)

// Description of OpenHome services
static string ohDesc(
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Product:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Product</serviceId>"
    "  <SCPDURL>/upmpd/OHProduct.xml</SCPDURL>"
    "  <controlURL>/ctl/OHProduct</controlURL>"
    "  <eventSubURL>/evt/OHProduct</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Info:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Info</serviceId>"
    "  <SCPDURL>/upmpd/OHInfo.xml</SCPDURL>"
    "  <controlURL>/ctl/OHInfo</controlURL>"
    "  <eventSubURL>/evt/OHInfo</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Time:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Time</serviceId>"
    "  <SCPDURL>/upmpd/OHTime.xml</SCPDURL>"
    "  <controlURL>/ctl/OHTime</controlURL>"
    "  <eventSubURL>/evt/OHTime</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Volume:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Volume</serviceId>"
    "  <SCPDURL>/upmpd/OHVolume.xml</SCPDURL>"
    "  <controlURL>/ctl/OHVolume</controlURL>"
    "  <eventSubURL>/evt/OHVolume</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Playlist:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Playlist</serviceId>"
    "  <SCPDURL>/upmpd/OHPlaylist.xml</SCPDURL>"
    "  <controlURL>/ctl/OHPlaylist</controlURL>"
    "  <eventSubURL>/evt/OHPlaylist</eventSubURL>"
    "</service>"
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Radio:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Radio</serviceId>"
    "  <SCPDURL>/upmpd/OHRadio.xml</SCPDURL>"
    "  <controlURL>/ctl/OHRadio</controlURL>"
    "  <eventSubURL>/evt/OHRadio</eventSubURL>"
    "</service>"
    );

// We only advertise the Openhome Receiver service if the sc2mpd
// songcast-to-mpd gateway command is available
static string ohDescReceive(
    "<service>"
    "  <serviceType>urn:av-openhome-org:service:Receiver:1</serviceType>"
    "  <serviceId>urn:av-openhome-org:serviceId:Receiver</serviceId>"
    "  <SCPDURL>/upmpd/OHReceiver.xml</SCPDURL>"
    "  <controlURL>/ctl/OHReceiver</controlURL>"
    "  <eventSubURL>/evt/OHReceiver</eventSubURL>"
    "</service>"
    );

static const string iconDesc(
    "<iconList>"
    "  <icon>"
    "    <mimetype>image/png</mimetype>"
    "    <width>64</width>"
    "    <height>64</height>"
    "    <depth>32</depth>"
    "    <url>/upmpd/icon.png</url>"
    "  </icon>"
    "</iconList>"
    );

static const string presDesc(
    "<presentationURL>/upmpd/presentation.html</presentationURL>"
    );

// description file fragment for Media Server as embedded device
static const string embedms_desc(
    "<device>"
    "<deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>"
    "<friendlyName>@FRIENDLYNAMEMEDIA@</friendlyName>"
    "<UDN>uuid:@UUIDMEDIA@</UDN>"
    "<serviceList>"
    "<service>"
    "<serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>"
    "<serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>"
    "<SCPDURL>/upmpd/ConnectionManager.xml</SCPDURL>"
    "<controlURL>/ctl1/ConnectionManager</controlURL>"
    "<eventSubURL>/evt1/ConnectionManager</eventSubURL>"
    "</service>"
    "<service>"
    "<serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>"
    "<serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>"
    "<SCPDURL>/upmpd/ContentDirectory.xml</SCPDURL>"
    "<controlURL>/ctl1/ContentDirectory</controlURL>"
    "<eventSubURL>/evt1/ContentDirectory</eventSubURL>"
    "</service>"
    "</serviceList>"
    "</device>"
    );

// Description file for Media Server as root device
static const string msonly_desc(
    "<?xml version=\"1.0\"?>"
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
    "<specVersion><major>1</major><minor>0</minor></specVersion>"
    "<device>"
    "<deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>"
    "<friendlyName>@FRIENDLYNAMEMEDIA@</friendlyName>"
    "<UDN>uuid:@UUIDMEDIA@</UDN>"
    "@ICONLIST@"
    "<serviceList><service>"
    "<serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>"
    "<serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>"
    "<SCPDURL>/upmpd/ConnectionManager.xml</SCPDURL>"
    "<controlURL>/ctl1/ConnectionManager</controlURL>"
    "<eventSubURL>/evt1/ConnectionManager</eventSubURL>"
    "</service><service>"
    "<serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>"
    "<serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>"
    "<SCPDURL>/upmpd/ContentDirectory.xml</SCPDURL>"
    "<controlURL>/ctl1/ContentDirectory</controlURL>"
    "<eventSubURL>/evt1/ContentDirectory</eventSubURL>"
    "</service></serviceList>"
    "</device>"
    "</root>"
    );

// The base XML description files. !Keep description.xml first!
static vector<const char *> xmlfilenames = 
{
    /* keep first */ "description.xml", /* keep first */
    "RenderingControl.xml", "AVTransport.xml", "ConnectionManager.xml",
    "ContentDirectory.xml"
};

// Optional OpenHome service description files
static vector<const char *> ohxmlfilenames = 
{
    "OHProduct.xml", "OHInfo.xml", "OHTime.xml", "OHVolume.xml", 
    "OHPlaylist.xml", "OHRadio.xml"
};

// Read and setup our (mostly XML) data to make it available from the
// virtual directory
bool initHttpFs(unordered_map<string, VDirContent>& files,
                const string& datadir,
                const UDevIds& ids,
                bool enableAV, bool enableOH, bool enableReceiver,
                bool enableMediaServer, bool msonly,
                const string& iconpath, const string& presentationhtml)
{
    if (msonly) {
        enableAV=enableOH=enableReceiver=false;
        enableMediaServer = true;
    }

    if (enableOH) {
        if (enableReceiver) {
            ohxmlfilenames.push_back("OHReceiver.xml");
        }
        xmlfilenames.insert(xmlfilenames.end(), ohxmlfilenames.begin(),
                            ohxmlfilenames.end());
    }
    
    string reason;
    string icondata;
    if (!iconpath.empty()) {
        if (!file_to_string(iconpath, icondata, &reason)) {
            if (iconpath.compare("/usr/share/upmpdcli/icon.png")) {
                LOGERR("Failed reading " << iconpath << " : " << reason << endl);
            } else {
                LOGDEB("Failed reading " << iconpath << " : " << reason << endl);
            }
        }
    }
    string presentationdata;
    if (!presentationhtml.empty()) {
        if (!file_to_string(presentationhtml, presentationdata, &reason)) {
            LOGERR("Failed reading " << presentationhtml << " : " << reason <<
                   endl);
        }
    }

    string dir("/upmpd/");
    for (unsigned int i = 0; i < xmlfilenames.size(); i++) {
        string filename = path_cat(datadir, xmlfilenames[i]);
        string data;
        if (!file_to_string(filename, data, &reason)) {
            LOGFAT("Failed reading " << filename << " : " << reason << endl);
            return false;
        }
        if (i == 0) {
            // Description
            if (!msonly) {
                // Set UUID and friendlyname for renderer
                data = regsub1("@UUID@", data, ids.uuid);
                data = regsub1("@FRIENDLYNAME@", data, ids.fname);
            }

            if (enableMediaServer && !msonly) {
                // Edit embedded media server description and
                // subsitute it in main description
                string msdesc = regsub1("@UUIDMEDIA@", embedms_desc, ids.uuidMS);
                msdesc = regsub1("@FRIENDLYNAMEMEDIA@", msdesc, ids.fnameMS);
                data = regsub1("@MEDIASERVER@", data, msdesc);
            } else if (msonly) {
                // Substitute values in msonly description
                data = regsub1("@UUIDMEDIA@", msonly_desc, ids.uuidMS);
                data = regsub1("@FRIENDLYNAMEMEDIA@", data, ids.fnameMS);
            } else {
                // No media server: erase the section
                data = regsub1("@MEDIASERVER@", data, "");
            }

            if (enableAV) {
                data = regsub1("@UPNPAV@", data, upnpAVDesc);
            } else {
                data = regsub1("@UPNPAV@", data, "");
            }

            if (enableOH) {
                if (enableReceiver) {
                    ohDesc += ohDescReceive;
                }
                data = regsub1("@OPENHOME@", data, ohDesc);
            } else {
                data = regsub1("@OPENHOME@", data, "");
            }

            if (!icondata.empty())
                data = regsub1("@ICONLIST@", data, iconDesc);
            else
                data = regsub1("@ICONLIST@", data, "");
            if (!presentationdata.empty())
                data = regsub1("@PRESENTATION@", data, presDesc);
            else
                data = regsub1("@PRESENTATION@", data, "");
        } // End description file editing

        files.insert(pair<string, VDirContent>
                     (dir + xmlfilenames[i], 
                      VDirContent(data, "application/xml")));
    }

    if (!icondata.empty()) {
        files.insert(pair<string, VDirContent>
                     (dir + "icon.png", 
                      VDirContent(icondata, "image/png")));
    }
    if (!presentationdata.empty()) {
        files.insert(pair<string, VDirContent>
                     (dir + "presentation.html", 
                      VDirContent(presentationdata, "text/html")));
    }
    return true;
}
