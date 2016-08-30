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
#ifndef _PLGWITHSLAVE_H_INCLUDED_
#define _PLGWITHSLAVE_H_INCLUDED_

#include <vector>

#include "cdplugin.hxx"
#include "libupnpp/device/vdir.hxx"

// Tidal interface
class PlgWithSlave : public CDPlugin {
public:
    PlgWithSlave(const std::string& name, CDPluginServices *services);
    virtual ~PlgWithSlave();

    // Returns totalmatches
    virtual int browse(
	const std::string& objid, int stidx, int cnt,
	std::vector<UpSong>& entries,
	const std::vector<std::string>& sortcrits = std::vector<std::string>(),
	BrowseFlag flg = BFChildren);

    virtual int search(
	const std::string& ctid, int stidx, int cnt,
	const std::string& searchstr,
	std::vector<UpSong>& entries,
	const std::vector<std::string>& sortcrits = std::vector<std::string>());

    virtual std::string get_media_url(const std::string& path);

    class Internal;
private:
    Internal *m;
};

#endif /* _PLGWITHSLAVE_H_INCLUDED_ */
