/* Copyright (C) 2016 J.F.Dockes
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
#ifndef _CDPLUGIN_H_INCLUDED_
#define _CDPLUGIN_H_INCLUDED_

#include <string>
#include "upmpdutils.hxx"
#include "libupnpp/device/vdir.hxx"

class CDPlugin;
class ConfSimple;

/// Service/Environment interface for media server modules
class CDPluginServices {
public:
    /// Returns something like "/tidal" (no end slash)
    virtual std::string getpathprefix(CDPlugin *) = 0;

    /// Returns the plugin to which belongs the parameter path, based
    /// on the path prefix above
    virtual CDPlugin *getpluginforpath(const std::string& path) = 0;
    
    /// Retrieve the IP address and port for the libupnp server. URLs
    /// intended to be served this way (by adding a vdir) should use
    /// these as host/port
    virtual std::string getupnpaddr(CDPlugin *) = 0;
    virtual int getupnpport(CDPlugin *) = 0;

    /// Add a virtual directory and set file operation interface. path
    /// must be equal or begin with the pathprefix.
    virtual bool setfileops(CDPlugin *, const std::string& path,
                            UPnPProvider::VirtualDir::FileOps ops)= 0;

    /// Get a pointer to the the main configuration file contents.
    virtual ConfSimple *getconfig(CDPlugin *)= 0;

    /// This calls plg->getname() and returns something like
    /// datadir/nm/nm-app.py. Can't see why this is in contentdirectory
    /// since it seems very specific to plgwithslave...
    virtual std::string getexecpath(CDPlugin *)= 0;
};

/// Interface to media server modules
///
/// The main operations, Browse and Search, return content as UpSong
/// structures. At the very minimum, id, parentid, iscontainer and
/// title should be properly set for each entry. Id is very important for
/// directories (containers), because this is the main parameter to
/// the browse call. title is what gets displayed in lists by the
/// control point.
///
/// The rest of the UpSong fields are only meaningful for items
/// (tracks), and only one is mandatory: uri.
///
/// The item uri will be used by the renderer for fetching the data to
/// be played. Three types can be used:
/// - A direct URL, usable by the renderer for fetching the data from
///   wherever it is (for example a radio stream HTTP URL).
/// - A vdir URL: this should have a host/port part matching the media
///   server host/port (which can be obtained through the environment
///   interface). Data will be fetched by libupnp using the vdir
///   interface. This is simple but limited: the libupnp HTTP server
///   does not emit "accept-range" headers, meaning that most CPs will
///   not try range requests, and that seeking will not work. No
///   redirects are possible. The URL path part should begin with the
///   pathprefix, which is how the request will be routed to the
///   appropriate plugin.
/// - A libmicrohttpd URL. A plugin may start a microhttpd instance,
///   and receive the connections directly. The host/port will be
///   obtained from the configuration. The advantage is that redirects
///   and range requests are possible, but it is a bit more
///   complicated as some libmicrohttpd knowledge is required.
class CDPlugin {
public:
    CDPlugin(const std::string& name, CDPluginServices *services)
        : m_name(name), m_services(services) {
    }
    virtual ~CDPlugin() {
    }

    /// List children or return metadata for target object. You can
    /// probably get by without implementing BFMeta in most cases.
    enum BrowseFlag {BFChildren, BFMeta};

    /// Browse object at objid, which should be a container, and
    /// return its content. 
    ///
    /// This reflects an UPnP Browse action. Refer to UPnP documentation.
    /// 
    /// @param objid the object to browse. The top directory for a plugin will
    //     be '0$plugin_name$', e.g. '0$qobuz$'
    /// @param stidx first entry to return.
    /// @param cnt number of entries.
    /// @param[output] entries output content.
    /// @param sortcrits csv list of sort criteria.
    /// @param flg browse flag
    /// @return total number of matched entries in container
    virtual int browse(
	const std::string& objid, int stidx, int cnt,
	std::vector<UpSong>& entries,
	const std::vector<std::string>& sortcrits = std::vector<std::string>(),
	BrowseFlag flg = BFChildren) = 0;

    /// Search under object at objid.
    ///
    /// This reflects an UPnP Search action, refer to UPnP
    /// documentation. Most plugins won't be able to actually perform
    /// the search under container operation. Plain search should be
    /// good enough.
    /// 
    /// @param objid the object to search
    /// @param stidx first entry to return.
    /// @param cnt number of entries.
    /// @param[output] entries output content.
    /// @param sortcrits csv list of sort criteria.
    /// @return total number of matched entries in container
    virtual int search(
	const std::string& ctid, int stidx, int cnt,
	const std::string& searchstr,
	std::vector<UpSong>& entries,
	const std::vector<std::string>& sortcrits = std::vector<std::string>())
    = 0;

    const std::string& getname() {
        return m_name;
    }

    virtual bool startInit() = 0;

    std::string m_name;
    CDPluginServices *m_services;
};

#endif /* _CDPLUGIN_H_INCLUDED_ */
