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
#ifndef _OHSERVICE_H_X_INCLUDED_
#define _OHSERVICE_H_X_INCLUDED_

#include <string>         
#include <unordered_map>  
#include <vector>         

#include "libupnpp/device/device.hxx"
#include "upmpd.hxx"

using namespace UPnPP;

// A parent class for all openhome service, to share a bit of state
// variable and event management code.
class OHService : public UPnPProvider::UpnpService {
public:
    OHService(const std::string& servtp, const std::string &servid, UpMpd *dev)
        : UpnpService(servtp, servid, dev), m_dev(dev) {
    }
    virtual ~OHService() { }

    // Implementation in upmpd.cxx
    virtual bool getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values);

protected:
    virtual bool makestate(std::unordered_map<std::string, std::string> &) = 0;
    // State variable storage
    std::unordered_map<std::string, std::string> m_state;
    UpMpd *m_dev;
};

#endif /* _OHSERVICE_H_X_INCLUDED_ */
