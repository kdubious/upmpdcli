#ifndef _HTTPFS_H_X_INCLUDED_
#define _HTTPFS_H_X_INCLUDED_
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

#include <string>
#include <unordered_map>

#include "libupnpp/device/device.hxx"

// Ad-hoc struct to hold the input identification parameters for the device(s)
struct UDevIds {
    std::string fname;
    std::string uuid;
    std::string fnameMS;
    std::string uuidMS;
};

/** 
 * Initialize the files which will be served by libupnp HTTP server.
 *
 * The output files are the device and service description xml
 * files. Some of the content is constant, some depends on data
 * elements and what services are actually enabled. The source XML files which 
 * we include or modify are found in @param datadir.
 * @param[output] files "file system" content as required by a
 *    UPnPProvider::UpnpDevice constructor.
 * @param datadir The location for the source files.
 * @param ids the friendly names and uuids for the devices, for embedding 
 *    in desc files
 */
extern bool initHttpFs(
    std::unordered_map<std::string, UPnPProvider::VDirContent>& files,
    const std::string& datadir, const UDevIds& ids,
    bool enableAV, bool enableOH, bool enableReceiver,
    bool enableMediaServer, bool msonly,
    const std::string& iconpath, const std::string& presentationhtml
    );

#endif /* _HTTPFS_H_X_INCLUDED_ */
