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

extern std::string g_configfilename;
extern std::string g_datadir;
class ConfSimple;
extern ConfSimple *g_config;
extern std::string g_protocolInfo;

typedef struct ohInfoDesc {
  std::string name;
  std::string info;
  std::string url;
  std::string imageUri;
} ohInfoDesc_t;

typedef struct ohProductDesc {
  ohInfoDesc_t manufacturer;
  ohInfoDesc_t model;
  ohInfoDesc_t product;
  std::string room;
} ohProductDesc_t;

using namespace UPnPProvider;

class UpMpdRenderCtl;
class UpMpdAVTransport;
class OHInfo;
class OHPlaylist;
class OHProduct;
class OHReceiver;
class SenderReceiver;
class OHRadio;

// The UPnP MPD frontend device with its services
class UpMpd : public UpnpDevice {
public:
    friend class UpMpdRenderCtl;
    friend class UpMpdAVTransport;
    friend class OHInfo;
    friend class OHPlaylist;
    friend class OHProduct;
    friend class OHReceiver;
    friend class OHVolume;
    friend class SenderReceiver;
    friend class OHRadio;
    
    enum OptFlags {
        upmpdNone = 0,
        // If set, the MPD queue belongs to us, we shall clear
        // it as we like.
        upmpdOwnQueue = 1, 
        // Export OpenHome services
        upmpdDoOH = 2,
        // Save queue metadata to disk for persistence across restarts
        // (mpd does it)
        upmpdOhMetaPersist = 4,
        // sc2mpd was found: advertise songcast receiver
        upmpdOhReceiver = 8,
        // Do not publish UPnP AV services (avtransport and renderer).
        upmpdNoAV = 16,
        // mpd2sc et al were found: advertise songcast sender/receiver mode
        upmpdOhSenderReceiver = 32,
    };
    struct Options {
        Options() : options(upmpdNone), ohmetasleep(0), schttpport(0),
                    sendermpdport(0) {}
        unsigned int options;
        std::string  cachefn;
        std::string  radioconf;
        unsigned int ohmetasleep;
        int schttpport;
        std::string scplaymethod;
        std::string sc2mpdpath;
        std::string senderpath;
        int sendermpdport;
    };
    UpMpd(const std::string& deviceid, const std::string& friendlyname,
          ohProductDesc_t& ohProductDesc,
          const std::unordered_map<std::string, VDirContent>& files,
          MPDCli *mpdcli, Options opts);
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
    UpMpdRenderCtl *m_rdctl;
    UpMpdAVTransport *m_avt;
    OHProduct *m_ohpr;
    OHPlaylist *m_ohpl;
    OHRadio *m_ohrd;
    OHInfo *m_ohif;
    OHReceiver *m_ohrcv;
    SenderReceiver *m_sndrcv;
    std::vector<UpnpService*> m_services;
    std::string m_friendlyname;
};

#endif /* _UPMPD_H_X_INCLUDED_ */
