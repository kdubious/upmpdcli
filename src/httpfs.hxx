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

#include "libupnpp/device/device.hxx"   // for VDirContent

extern bool initHttpFs(std::unordered_map<std::string, 
                                          UPnPProvider::VDirContent>& files,
                       const std::string& datadir,
                       const std::string& UUID, 
                       const std::string& friendlyname,
                       bool enableAV, bool enableOH, bool enableReceiver,
                       bool enableMediaServer, bool msonly,
                       const std::string& iconpath,
                       const std::string& presentationhtml
    );

inline std::string uuidMediaServer(const std::string& uuid) {
    return uuid + "-mediaserver";
}
inline std::string friendlyNameMediaServer(const std::string& f) {
    return f + "-mediaserver";
}
#endif /* _HTTPFS_H_X_INCLUDED_ */
