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

#include "upmpd.hxx"
#include "upmpdutils.hxx"
#include "httpfs.hxx"

using namespace std;
using namespace UPnPP;

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
// has a number of substitutable fields for optional data, like the
// description of OpenHome services
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

// The base XML description files. !Keep description.xml first!
static vector<const char *> xmlfilenames = 
{
    /* keep first */ "description.xml", /* keep first */
    "RenderingControl.xml", "AVTransport.xml", "ConnectionManager.xml",
};

// Optional OpenHome service description files
static vector<const char *> ohxmlfilenames = 
{
    "OHProduct.xml", "OHInfo.xml", "OHTime.xml", "OHVolume.xml", 
    "OHPlaylist.xml",
};

/** Read protocol info file. This contains the connection manager
 * protocol info data
 *
 * We strip white-space from beginning/ends of lines, and allow
 * #-started comments (on a line alone only, comments after data not allowed).
 */
static bool read_protocolinfo(const string& fn, string& out)
{
    ifstream input;
    input.open(fn, ios::in);
    if (!input.is_open()) {
	return false;
    }	    
    bool eof = false;
    for (;;) {
        string line;
	getline(input, line);
	if (!input.good()) {
	    if (input.bad()) {
		return false;
	    }
	    // Must be eof ? But maybe we have a partial line which
	    // must be processed. This happens if the last line before
	    // eof ends with a backslash, or there is no final \n
            eof = true;
	}
        trimstring(line, " \t\n\r");
        if (line[0] == '#')
            continue;
        out += line;
        if (eof) 
            break;
    }
    return true;
}


// Read and setup our (mostly XML) data to make it available from the
// virtual directory
bool initHttpFs(unordered_map<string, VDirContent>& files,
                const string& datadir,
                const string& UUID, const string& friendlyname, 
                bool enableAV, bool enableOH, 
                const string& iconpath, const string& presentationhtml)
{
    if (enableOH) {
        if (!g_sc2mpd_path.empty()) {
            ohxmlfilenames.push_back("OHReceiver.xml");
        }
        xmlfilenames.insert(xmlfilenames.end(), ohxmlfilenames.begin(),
                            ohxmlfilenames.end());
    }
    
    string protofile(path_cat(datadir, "protocolinfo.txt"));
    if (!read_protocolinfo(protofile, g_protocolInfo)) {
        LOGFAT("Failed reading protocol info from " << protofile << endl);
        return false;
    }

    string reason;
    string icondata;
    if (!iconpath.empty()) {
        if (!file_to_string(iconpath, icondata, &reason)) {
            LOGERR("Failed reading " << iconpath << " : " << reason << endl);
        }
    }
    string presentationdata;
    if (!presentationhtml.empty()) {
        if (!file_to_string(presentationhtml, presentationdata, &reason)) {
            LOGERR("Failed reading " << iconpath << " : " << reason << endl);
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
            // Special for description: set UUID and friendlyname
            data = regsub1("@UUID@", data, UUID);
            data = regsub1("@FRIENDLYNAME@", data, friendlyname);
            if (enableAV) {
                data = regsub1("@UPNPAV@", data, upnpAVDesc);
            } else {
                data = regsub1("@UPNPAV@", data, "");
            }
            if (enableOH) {
                if (!g_sc2mpd_path.empty()) {
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
        }
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
