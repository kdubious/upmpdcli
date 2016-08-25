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

// Interface for media server modules
class CDPlugin {
public:
    CDPlugin() {
    }
    virtual ~CDPlugin() {
    }

    /// List children or return metadata for target object. You can
    /// probably get by without implementing BFMeta in most cases.
    enum BrowseFlag {BFChildren, BFMeta};

    /// Browse object at objid, which should be a container, and
    /// return its content. 
    ///
    /// This reflects an UPnP Browse action, refer to UPnP documentation.
    /// 
    /// @param objid the object to browse.
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
};

#endif /* _CDPLUGIN_H_INCLUDED_ */
