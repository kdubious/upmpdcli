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
#ifndef _OHRECEIVER_H_X_INCLUDED_
#define _OHRECEIVER_H_X_INCLUDED_

#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector
#include <memory>

#include "libupnpp/device/device.hxx"   // for UpnpService
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, SoapOutgoing

#include "execmd.h"

using namespace UPnPP;
class UpMpd;
class OHPlaylist;

class OHReceiver : public UPnPProvider::UpnpService {
public:
    OHReceiver(UpMpd *dev, OHPlaylist *pl, int httpport);

    virtual bool getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values);

    virtual bool iStop();

private:
    int play(const SoapIncoming& sc, SoapOutgoing& data);
    int stop(const SoapIncoming& sc, SoapOutgoing& data);
    int setSender(const SoapIncoming& sc, SoapOutgoing& data);
    int sender(const SoapIncoming& sc, SoapOutgoing& data);
    int protocolInfo(const SoapIncoming& sc, SoapOutgoing& data);
    int transportState(const SoapIncoming& sc, SoapOutgoing& data);

    bool makestate(std::unordered_map<std::string, std::string> &st);
    void maybeWakeUp(bool ok);
    // State variable storage (previous state)
    std::unordered_map<std::string, std::string> m_state;
    // Current
    std::string m_uri;
    std::string m_metadata;

    UpMpd *m_dev;
    OHPlaylist *m_pl;

    std::shared_ptr<ExecCmd> m_cmd;
    int m_httpport;
    std::string m_httpuri;
};

#endif /* _OHRECEIVER_H_X_INCLUDED_ */
