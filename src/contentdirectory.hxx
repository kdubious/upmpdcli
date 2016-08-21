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

using namespace UPnPP;

class ContentDirectory : public UPnPProvider::UpnpService {
public:
    ContentDirectory(UPnPProvider::UpnpDevice *dev);
    ~ContentDirectory();
    
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

