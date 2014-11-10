/* Copyright (C) 2014 J.F.Dockes
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
#ifndef _OHTIME_H_X_INCLUDED_
#define _OHTIME_H_X_INCLUDED_

#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector

#include "libupnpp/device/device.hxx"   // for UpnpService
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, SoapOutgoing

class UpMpd;

using namespace UPnPP;

class OHTime : public UPnPProvider::UpnpService {
public:
    OHTime(UpMpd *dev);

    virtual bool getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values);
private:
    int ohtime(const SoapIncoming& sc, SoapOutgoing& data);

    void getdata(std::string& trackcount, std::string &duration, std::string& seconds);
    bool makestate(std::unordered_map<std::string, std::string> &st);
    // State variable storage
    std::unordered_map<std::string, std::string> m_state;
    UpMpd *m_dev;
};

#endif /* _OHTIME_H_X_INCLUDED_ */
