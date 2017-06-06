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

#include "upmpd.hxx"

#include "libupnpp/device/device.hxx"   // for UpnpDevice, UpnpService
#include "libupnpp/log.hxx"             // for LOGFAT, LOGERR, Logger, etc
#include "libupnpp/upnpplib.hxx"        // for LibUPnP
#include "libupnpp/control/cdircontent.hxx"

#include "smallut.h"
#include "avtransport.hxx"
#include "conman.hxx"
#include "mpdcli.hxx"
#include "ohinfo.hxx"
#include "ohplaylist.hxx"
#include "ohradio.hxx"
#include "ohproduct.hxx"
#include "ohreceiver.hxx"
#include "ohtime.hxx"
#include "ohvolume.hxx"
#include "renderctl.hxx"
#include "upmpdutils.hxx"
#include "execmd.h"
#include "httpfs.hxx"
#include "ohsndrcv.hxx"
#include "protocolinfo.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

// Note: if we ever need this to work without cxx11, there is this:
// http://www.tutok.sk/fastgl/callback.html
UpMpd::UpMpd(const string& deviceid, const string& friendlyname,
             ohProductDesc_t& ohProductDesc,
             const unordered_map<string, VDirContent>& files,
             MPDCli *mpdcli, Options opts)
    : UpnpDevice(deviceid, files), m_mpdcli(mpdcli), m_mpds(0),
      m_options(opts.options),
      m_mcachefn(opts.cachefn),
      m_rdctl(0), m_avt(0), m_ohpr(0), m_ohpl(0), m_ohrd(0), m_ohrcv(0),
      m_sndrcv(0), m_friendlyname(friendlyname)
{
    bool avtnoev = (m_options & upmpdNoAV) != 0; 
    // Note: the order is significant here as it will be used when
    // calling the getStatus() methods, and we want AVTransport to
    // update the mpd status for everybody
    m_avt = new UpMpdAVTransport(this, avtnoev);
    m_services.push_back(m_avt);
    m_rdctl = new UpMpdRenderCtl(this, avtnoev);
    m_services.push_back(m_rdctl);
    m_services.push_back(new UpMpdConMan(this));

    if (m_options & upmpdDoOH) {
        m_ohif = new OHInfo(this);
        m_services.push_back(m_ohif);
        m_services.push_back(new OHTime(this));
        m_services.push_back(new OHVolume(this));
        m_ohpl = new OHPlaylist(this, opts.ohmetasleep);
        m_services.push_back(m_ohpl);
        if (m_avt)
            m_avt->setOHP(m_ohpl);
        if (m_ohif)
            m_ohif->setOHPL(m_ohpl);
        m_ohrd = new OHRadio(this);
        if (m_ohrd && !m_ohrd->ok()) {
            delete m_ohrd;
            m_ohrd = 0;
        }
        if (m_ohrd)
            m_services.push_back(m_ohrd);
        if (m_options & upmpdOhReceiver) {
            struct OHReceiverParams parms;
            if (opts.schttpport)
                parms.httpport = opts.schttpport;
            if (!opts.scplaymethod.empty()) {
                if (!opts.scplaymethod.compare("alsa")) {
                    parms.pm = OHReceiverParams::OHRP_ALSA;
                } else if (!opts.scplaymethod.compare("mpd")) {
                    parms.pm = OHReceiverParams::OHRP_MPD;
                }
            }
            parms.sc2mpdpath = opts.sc2mpdpath;
            m_ohrcv = new OHReceiver(this, parms);
            m_services.push_back(m_ohrcv);
        }
        if (m_options& upmpdOhSenderReceiver) {
            // Note: this is not an UPnP service
            m_sndrcv = new SenderReceiver(this, opts.senderpath,
                                          opts.sendermpdport);
        }
        // Create ohpr last, so that it can ask questions to other services
        m_ohpr = new OHProduct(this, ohProductDesc);
        m_services.push_back(m_ohpr);
    }
}

UpMpd::~UpMpd()
{
    delete m_sndrcv;
    for (vector<UpnpService*>::iterator it = m_services.begin();
         it != m_services.end(); it++) {
        delete(*it);
    }
}

const MpdStatus& UpMpd::getMpdStatus()
{
    m_mpds = &m_mpdcli->getStatus();
    return *m_mpds;
}

bool UpMpd::checkContentFormat(const string& uri, const string& didl,
                               UpSong *ups)
{
    UPnPClient::UPnPDirContent dirc;
    if (!dirc.parse(didl) || dirc.m_items.size() == 0) {
        LOGERR("checkContentFormat: didl parse failed\n");
        if ((m_options & upmpdNoContentFormatCheck)) {
            noMetaUpSong(ups);
            return true;
        } else {
            return false;
        }
    }
    UPnPClient::UPnPDirObject& dobj = *dirc.m_items.begin();

    if ((m_options & upmpdNoContentFormatCheck)) {
        LOGERR("checkContentFormat: format check disabled\n");
        return dirObjToUpSong(dobj, ups);
    }
    
    const std::unordered_set<std::string>& supportedformats =
        Protocolinfo::the()->getsupportedformats();

    for (vector<UPnPClient::UPnPResource>::const_iterator it =
             dobj.m_resources.begin(); it != dobj.m_resources.end(); it++) {
        if (!it->m_uri.compare(uri)) {
            ProtocolinfoEntry e;
            if (!it->protoInfo(e)) {
                LOGERR("checkContentFormat: resource has no protocolinfo\n");
                return false;
            }
            string cf = e.contentFormat;
            if (supportedformats.find(cf) == supportedformats.end()) { // 
                LOGERR("checkContentFormat: unsupported:: " << cf << endl);
                return false;
            } else {
                LOGDEB("checkContentFormat: supported: " << cf << endl);
                if (ups) {
                    return dirObjToUpSong(dobj, ups);
                } else {
                    return true;
                }
            }
        }
    }
    LOGERR("checkContentFormat: uri not found in metadata resource list\n");
    return false;
}
