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
#ifndef _OHPRODUCT_H_X_INCLUDED_
#define _OHPRODUCT_H_X_INCLUDED_

#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/device/device.hxx"   // for UpnpService
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, SoapOutgoing

class UpMpd;

using namespace UPnPP;

class OHProduct : public UPnPProvider::UpnpService {
public:
    OHProduct(UpMpd *dev, const std::string& friendlyname, bool hasRcv);

    virtual bool getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values);
    int iSetSourceIndex(int index);
private:
    int manufacturer(const SoapIncoming& sc, SoapOutgoing& data);
    int model(const SoapIncoming& sc, SoapOutgoing& data);
    int product(const SoapIncoming& sc, SoapOutgoing& data);
    int standby(const SoapIncoming& sc, SoapOutgoing& data);
    int setStandby(const SoapIncoming& sc, SoapOutgoing& data);
    int sourceCount(const SoapIncoming& sc, SoapOutgoing& data);
    int sourceXML(const SoapIncoming& sc, SoapOutgoing& data);
    int sourceIndex(const SoapIncoming& sc, SoapOutgoing& data);
    int setSourceIndex(const SoapIncoming& sc, SoapOutgoing& data);
    int setSourceIndexByName(const SoapIncoming& sc, SoapOutgoing& data);
    int source(const SoapIncoming& sc, SoapOutgoing& data);
    int attributes(const SoapIncoming& sc, SoapOutgoing& data);
    int sourceXMLChangeCount(const SoapIncoming& sc, SoapOutgoing& data);

    UpMpd *m_dev;
    std::string m_roomOrName;
    int m_sourceIndex;
    bool m_standby;
};

#endif /* _OHPRODUCT_H_X_INCLUDED_ */
