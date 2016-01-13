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
#ifndef _OHRADIO_H_X_INCLUDED_
#define _OHRADIO_H_X_INCLUDED_

#include <string>
#include <unordered_map>
#include <vector>

#include "libupnpp/device/device.hxx"
#include "libupnpp/soaphelp.hxx"
#include "mpdcli.hxx"

class UpMpd;

using namespace UPnPP;

class OHRadio : public UPnPProvider::UpnpService {
public:
    OHRadio(UpMpd *dev);

    virtual bool getEventData(bool all, std::vector<std::string>& names,
                              std::vector<std::string>& values);
    int iStop();

    // Source active ?
    void setActive(bool onoff);

private:
    int channel(const SoapIncoming& sc, SoapOutgoing& data);
    int channelsMax(const SoapIncoming& sc, SoapOutgoing& data);
    int id(const SoapIncoming& sc, SoapOutgoing& data);
    int idArray(const SoapIncoming& sc, SoapOutgoing& data);
    int idArrayChanged(const SoapIncoming& sc, SoapOutgoing& data);
    int pause(const SoapIncoming& sc, SoapOutgoing& data);
    int play(const SoapIncoming& sc, SoapOutgoing& data);
    int protocolInfo(const SoapIncoming& sc, SoapOutgoing& data);
    int ohread(const SoapIncoming& sc, SoapOutgoing& data);
    int readList(const SoapIncoming& sc, SoapOutgoing& data);
    int seekSecondAbsolute(const SoapIncoming& sc, SoapOutgoing& data);
    int seekSecondRelative(const SoapIncoming& sc, SoapOutgoing& data);
    int setChannel(const SoapIncoming& sc, SoapOutgoing& data);
    int setId(const SoapIncoming& sc, SoapOutgoing& data);
    int stop(const SoapIncoming& sc, SoapOutgoing& data);
    int transportState(const SoapIncoming& sc, SoapOutgoing& data);

    std::string metaForId(unsigned int id);
    void readRadios();
    int setPlaying(const std::string& uri);
    bool makeIdArray(std::string&);
    bool makestate(std::unordered_map<std::string, std::string>& st);
    void maybeWakeUp(bool ok);

    // State variable storage
    std::unordered_map<std::string, std::string> m_state;
    UpMpd *m_dev;
    bool m_active;
    unsigned int m_id; // Current channel id
    int m_songid; // MPD song id for the radio uri, or 0
};

#endif /* _OHRADIO_H_X_INCLUDED_ */
