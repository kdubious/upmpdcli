/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <string>
#include <functional>

using namespace std;
using namespace std::placeholders;

#include <upnp/upnp.h>

#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/control/renderingcontrol.hxx"
#include "libupnpp/control/avlastchg.hxx"

namespace UPnPClient {

const string 
RenderingControl::SType("urn:schemas-upnp-org:service:RenderingControl:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool RenderingControl::isRDCService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

void RenderingControl::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("RenderingControl::evtCallback: m_reporter " << m_reporter << endl);
    for (auto it = props.begin(); it != props.end(); it++) {
        if (!it->first.compare("LastChange")) {
            std::unordered_map<std::string, std::string> props1;
            if (!decodeAVLastChange(it->second, props1)) {
                LOGERR("RenderingControl::evtCallback: bad LastChange value: "
                       << it->second << endl);
                return;
            }
            for (auto it1 = props1.begin(); it1 != props1.end(); it1++) {
                LOGDEB1("    " << it1->first << " -> " << 
                        it1->second << endl);
                if (!it1->first.compare("Volume")) {
                    if (m_reporter) {
                        m_reporter->changed(it1->first.c_str(),
                                            atoi(it1->second.c_str()));
                    }
                } else if (!it1->first.compare("Mute")) {
                    bool mute;
                    if (m_reporter && stringToBool(it1->second, &mute))
                        m_reporter->changed(it1->first.c_str(), mute);
                }
            }
        } else {
            LOGINF("RenderingControl:event: var not lastchange: "
                   << it->first << " -> " << it->second << endl;);
        }
    }
}

void RenderingControl::registerCallback()
{
    Service::registerCallback(bind(&RenderingControl::evtCallback, this, _1));
}

int RenderingControl::setVolume(int volume, const string& channel)
{
    SoapEncodeInput args(m_serviceType, "SetVolume");
    args("Channel", channel)("DesiredVolume", SoapHelp::i2s(volume));
    SoapDecodeOutput data;
    return runAction(args, data);
}

int RenderingControl::getVolume(const string& channel)
{
    SoapEncodeInput args(m_serviceType, "GetVolume");
    args("Channel", channel);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int volume;
    if (!data.getInt("CurrentVolume", &volume)) {
        LOGERR("RenderingControl:getVolume: missing CurrentVolume in response" 
        << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return volume;
}
int RenderingControl::setMute(bool mute, const string& channel)
{
    SoapEncodeInput args(m_serviceType, "SetMute");
    args("Channel", channel)("DesiredMute", SoapHelp::i2s(mute?1:0));
    SoapDecodeOutput data;
    return runAction(args, data);
}

bool RenderingControl::getMute(const string& channel)
{
    SoapEncodeInput args(m_serviceType, "GetMute");
    args("Channel", channel);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    bool mute;
    if (!data.getBool("CurrentMute", &mute)) {
        LOGERR("RenderingControl:getMute: missing CurrentMute in response" 
        << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return mute;
}

} // End namespace UPnPClient

