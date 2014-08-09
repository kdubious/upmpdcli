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
#include "libupnpp/control/avtransport.hxx"
#include "libupnpp/control/avlastchg.hxx"

namespace UPnPClient {

const string AVTransport::SType("urn:schemas-upnp-org:service:AVTransport:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool AVTransport::isAVTService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

void AVTransport::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    //LOGDEB("AVTransport::evtCallback:" << endl);
    for (auto& entry: props) {
        if (entry.first.compare("LastChange")) {
            LOGINF("AVTransport:event: var not lastchange: "
                   << entry.first << " -> " << entry.second << endl;);
            continue;
        }

        std::unordered_map<std::string, std::string> props1;
        if (!decodeAVLastChange(entry.second, props1)) {
            LOGERR("AVTransport::evtCallback: bad LastChange value: "
                   << entry.second << endl);
            return;
        }
        for (auto& entry1: props1) {
            //LOGDEB("    " << entry1.first << " -> " << 
            //       entry1.second << endl);
            if (!entry1.first.compare("TransportState")) {
            } else if (!entry1.first.compare("CurrentTransportActions")) {
            } else if (!entry1.first.compare("TransportStatus")) {
            } else if (!entry1.first.compare("TransportPlaySpeed")) {
            } else if (!entry1.first.compare("CurrentTrack")) {
            } else if (!entry1.first.compare("CurrentTrackURI")) {
            } else if (!entry1.first.compare("CurrentTrackMetaData")) {
            } else if (!entry1.first.compare("NumberOfTracks")) {
            } else if (!entry1.first.compare("CurrentMediaDuration")) {
            } else if (!entry1.first.compare("CurrentTrackDuration")) {
            } else if (!entry1.first.compare("AVTransportURI")) {
            } else if (!entry1.first.compare("AVTransportURIMetaData")) {
            } else if (!entry1.first.compare("RelativeTimePosition")) {
            } else if (!entry1.first.compare("AbsoluteTimePosition")) {
            } else if (!entry1.first.compare("NextAVTransportURI")) {
            } else if (!entry1.first.compare("NextAVTransportURIMetaData")){
            } else if (!entry1.first.compare("PlaybackStorageMedium")) {
            } else if (!entry1.first.compare("PossiblePlaybackStorageMedium")) {
            } else if (!entry1.first.compare("RecordStorageMedium")) {
            } else if (!entry1.first.compare("RelativeCounterPosition")) {
            } else if (!entry1.first.compare("AbsoluteCounterPosition")) {
            } else if (!entry1.first.compare("CurrentPlayMode")) {
            } else if (!entry1.first.compare("PossibleRecordStorageMedium")) {
            } else if (!entry1.first.compare("RecordMediumWriteStatus")) {
            } else if (!entry1.first.compare("CurrentRecordQualityMode")) {
            } else if (!entry1.first.compare("PossibleRecordQualityModes")){
            }
        }
    }
}

void AVTransport::registerCallback()
{
    Service::registerCallback(bind(&AVTransport::evtCallback, this, _1));
}

} // End namespace UPnPClient

