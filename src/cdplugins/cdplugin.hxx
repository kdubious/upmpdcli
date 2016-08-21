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

    enum BrowseFlag {BFChildren, BFMeta};

    // Returns totalmatches
    virtual int browse(
	const std::string& objid, int stidx, int cnt,
	std::vector<UpSong>& entries,
	const std::vector<std::string>& sortcrits = std::vector<std::string>(),
	BrowseFlag flg = BFChildren) = 0;

};

#endif /* _CDPLUGIN_H_INCLUDED_ */
