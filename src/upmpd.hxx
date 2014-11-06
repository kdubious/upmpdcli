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

#ifndef _UPMPD_H_X_INCLUDED_
#define _UPMPD_H_X_INCLUDED_

#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector

#include "libupnpp/device/device.hxx"   // for UpnpDevice, etc

class MPDCli;
class MpdStatus;

using namespace UPnPProvider;

// The UPnP MPD frontend device with its services
class UpMpd : public UpnpDevice {
public:
    friend class UpMpdRenderCtl;
    friend class UpMpdAVTransport;
    friend class OHInfo;
    friend class OHPlaylist;
    friend class OHReceiver;

    enum Options {
        upmpdNone,
        // If set, the MPD queue belongs to us, we shall clear
        // it as we like.
        upmpdOwnQueue = 1, 
        // Export OpenHome services
        upmpdDoOH = 2,
        // Save queue metadata to disk for persistence across restarts
        // (mpd does it)
        upmpdOhMetaPersist = 4
    };
    UpMpd(const std::string& deviceid, const std::string& friendlyname,
          const std::unordered_map<std::string, VDirContent>& files,
          MPDCli *mpdcli, unsigned int opts, const std::string& cachefn,
        int schttpport);
    ~UpMpd();

    const MpdStatus &getMpdStatus();
    const MpdStatus &getMpdStatusNoUpdate()
        {
            if (m_mpds == 0)
                return getMpdStatus();
            else
                return *m_mpds;
        }

    const std::string& getMetaCacheFn()
        {
            return m_mcachefn;
        }

private:
    MPDCli *m_mpdcli;
    const MpdStatus *m_mpds;
    unsigned int m_options;
    std::string m_mcachefn;
    std::vector<UpnpService*> m_services;
};


extern std::string upmpdProtocolInfo;

#endif /* _UPMPD_H_X_INCLUDED_ */
