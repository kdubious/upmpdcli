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
#ifndef _CONTENTDIRECTORY_H_INCLUDED_
#define _CONTENTDIRECTORY_H_INCLUDED_

#include <string>
#include <vector>

#include "libupnpp/device/device.hxx"
#include "libupnpp/soaphelp.hxx"

#include "cdplugin.hxx"

using namespace UPnPP;

class ContentDirectory : public UPnPProvider::UpnpService,
                         public CDPluginServices {
public:
    ContentDirectory(UPnPProvider::UpnpDevice *dev);
    ~ContentDirectory();

    /// Returns something like "/tidal" (no end slash)
    virtual std::string getpathprefix(CDPlugin *);
    /// Return plugin based on path prefix
    CDPlugin *getpluginforpath(const std::string& path);

    /// Retrieve the IP address and port for the libupnp server. URLs
    /// intended to be served this way (by adding a vdir) should use
    /// these as host/port
    virtual std::string getupnpaddr(CDPlugin *);
    virtual int getupnpport(CDPlugin *);

    /// Add a virtual directory and set file operation interface. path
    /// must be equal or begin with the pathprefix.
    virtual bool setfileops(CDPlugin *, const std::string& path,
                            UPnPProvider::VirtualDir::FileOps ops);

    /// Access the main configuration file.
    virtual ConfSimple *getconfig(CDPlugin *);
    virtual std::string getexecpath(CDPlugin *);

    /// Check if the configuration indicates that the media server needs to be started.
    static bool mediaServerNeeded();
    
private:
    int actGetSearchCapabilities(const SoapIncoming& sc, SoapOutgoing& data);
    int actGetSortCapabilities(const SoapIncoming& sc, SoapOutgoing& data);
    int actGetSystemUpdateID(const SoapIncoming& sc, SoapOutgoing& data);
    int actBrowse(const SoapIncoming& sc, SoapOutgoing& data);
    int actSearch(const SoapIncoming& sc, SoapOutgoing& data);

    class Internal;
    Internal *m;
};
#endif

