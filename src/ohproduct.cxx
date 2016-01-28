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
#include "config.h"

#include "ohproduct.hxx"

#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS, etc

#include <functional>                   // for _Bind, bind, _1, _2
#include <iostream>                     // for endl, etc
#include <map>                          // for _Rb_tree_const_iterator, etc
#include <string>                       // for string, operator<<, etc
#include <utility>                      // for pair
#include <vector>                       // for vector
#include <utility>

#include "libupnpp/device/device.hxx"   // for UpnpService
#include "libupnpp/log.hxx"             // for LOGDEB
#include "libupnpp/soaphelp.hxx"        // for SoapOutgoing, SoapIncoming

#include "upmpd.hxx"
#include "upmpdutils.hxx"
#include "ohplaylist.hxx"
#include "ohradio.hxx"
#include "ohreceiver.hxx"
#include "ohsndrcv.hxx"
#include "ohinfo.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpProduct("urn:av-openhome-org:service:Product:1");
static const string sIdProduct("urn:av-openhome-org:serviceId:Product");

static string csxml("<SourceList>\n");

// (Type, Name) list
static vector<pair<string, string> > o_sources;

OHProduct::OHProduct(UpMpd *dev, const string& friendlyname)
    : UpnpService(sTpProduct, sIdProduct, dev), m_dev(dev),
      m_roomOrName(friendlyname), m_sourceIndex(0), m_standby(false)
{
    // Playlist must stay first.
    o_sources.push_back(pair<string,string>("Playlist","Playlist"));
    if (m_dev->m_ohrd) {
        o_sources.push_back(pair<string, string>("Radio", "Radio"));
    }
    if (m_dev->m_ohrcv) {
        o_sources.push_back(pair<string,string>("Receiver", "Receiver"));
        if (m_dev->m_sndrcv &&
            m_dev->m_ohrcv->playMethod() == OHReceiverParams::OHRP_ALSA) {
            // It might be possible to make things work with the MPD
            // play method but this would be complicated (the mpd we
            // want to get playing from sc2mpd HTTP is the
            // original/saved one, not the current one, which is doing
            // the playing and sending to the fifo, so we'd need to
            // tell ohreceiver about using the right one.
            o_sources.push_back(pair<string,string>("Playlist",
                                                    "SenderReceiverPL"));
            if (m_dev->m_ohrd) {
                o_sources.push_back(pair<string,string>("Radio",
                                                        "SenderReceiverRD"));
            }
        }
    }

    for (auto it = o_sources.begin(); it != o_sources.end(); it++) {
        string visible = it->first.compare("Receiver") ? "1" : "0";
        csxml += string(" <Source>\n") +
            "  <Name>" + it->second + "</Name>\n" +
            "  <Type>" + it->first + "</Type>\n" +
            "  <Visible>" + visible + "</Visible>\n" +
            "  </Source>\n";
    }
    csxml += string("</SourceList>\n");
    LOGDEB("OHProduct::OHProduct: sources: " << csxml << endl);

    dev->addActionMapping(this, "Manufacturer", 
                          bind(&OHProduct::manufacturer, this, _1, _2));
    dev->addActionMapping(this, "Model", bind(&OHProduct::model, this, _1, _2));
    dev->addActionMapping(this, "Product", 
                          bind(&OHProduct::product, this, _1, _2));
    dev->addActionMapping(this, "Standby", 
                          bind(&OHProduct::standby, this, _1, _2));
    dev->addActionMapping(this, "SetStandby", 
                          bind(&OHProduct::setStandby, this, _1, _2));
    dev->addActionMapping(this, "SourceCount", 
                          bind(&OHProduct::sourceCount, this, _1, _2));
    dev->addActionMapping(this, "SourceXml", 
                          bind(&OHProduct::sourceXML, this, _1, _2));
    dev->addActionMapping(this, "SourceIndex", 
                          bind(&OHProduct::sourceIndex, this, _1, _2));
    dev->addActionMapping(this, "SetSourceIndex", 
                          bind(&OHProduct::setSourceIndex, this, _1, _2));
    dev->addActionMapping(this, "SetSourceIndexByName", 
                          bind(&OHProduct::setSourceIndexByName, this, _1, _2));
    dev->addActionMapping(this, "Source", 
                          bind(&OHProduct::source, this, _1, _2));
    dev->addActionMapping(this, "Attributes", 
                          bind(&OHProduct::attributes, this, _1, _2));
    dev->addActionMapping(this, "SourceXmlChangeCount", 
                          bind(&OHProduct::sourceXMLChangeCount, this, _1, _2));
}

OHProduct::~OHProduct()
{
}

static const string csattrs("Info Time Volume");
static const string csversion(UPMPDCLI_PACKAGE_VERSION);
static const string csmanname("UpMPDCli heavy industries Co.");
static const string csmaninfo("Such nice guys and gals");
static const string csmanurl("http://www.lesbonscomptes.com/upmpdcli");
static const string csmodname("UpMPDCli UPnP-MPD gateway");
static const string csmodurl("http://www.lesbonscomptes.com/upmpdcli");
static const string csprodname("Upmpdcli");

bool OHProduct::makestate(unordered_map<string, string> &st)
{
    st.clear();

    st["ManufacturerName"] = csmanname;
    st["ManufacturerInfo"] = csmaninfo;
    st["ManufacturerUrl"] = csmanurl;
    st["ManufacturerImageUri"] = "";
    st["ModelName"] = csmodname;
    st["ModelInfo"] = csversion;
    st["ModelUrl"] = csmodurl;
    st["ModelImageUri"] = "";
    st["ProductRoom"] = m_roomOrName;
    st["ProductName"] = csprodname;
    st["ProductInfo"] = csversion;
    st["ProductUrl"] = "";
    st["ProductImageUri"] = "";
    st["Standby"] = m_standby ? "1" : "0";
    st["SourceCount"] = SoapHelp::i2s(o_sources.size());
    st["SourceXml"] = csxml;
    st["SourceIndex"] = SoapHelp::i2s(m_sourceIndex);
    st["Attributes"] = csattrs;

    return true;
}

bool OHProduct::getEventData(bool all, std::vector<std::string>& names, 
                             std::vector<std::string>& values)
{
    //LOGDEB("OHProduct::getEventData" << endl);

    unordered_map<string, string> state;
    makestate(state);

    unordered_map<string, string> changed;
    if (all) {
        changed = state;
    } else {
        changed = diffmaps(m_state, state);
    }
    m_state = state;

    for (auto it = changed.begin(); it != changed.end(); it++) {
        names.push_back(it->first);
        values.push_back(it->second);
    }

    return true;
}

int OHProduct::manufacturer(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::manufacturer" << endl);
    data.addarg("Name", csmanname);
    data.addarg("Info", csmaninfo);
    data.addarg("Url", csmanurl);
    data.addarg("ImageUri", "");
    return UPNP_E_SUCCESS;
}

int OHProduct::model(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::model" << endl);
    data.addarg("Name", csmodname);
    data.addarg("Info", csversion);
    data.addarg("Url", csmodurl);
    data.addarg("ImageUri", "");
    return UPNP_E_SUCCESS;
}

int OHProduct::product(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::product" << endl);
    data.addarg("Room", m_roomOrName);
    data.addarg("Name", csprodname);
    data.addarg("Info", csversion);
    data.addarg("Url", "");
    data.addarg("ImageUri", "");
    return UPNP_E_SUCCESS;
}

int OHProduct::standby(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::standby" << endl);
    data.addarg("Value", "0");
    return UPNP_E_SUCCESS;
}

int OHProduct::setStandby(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::setStandby" << endl);
    if (!sc.get("Value", &m_standby)) {
        return UPNP_E_INVALID_PARAM;
    }
    m_dev->loopWakeup();
    return UPNP_E_SUCCESS;
}

int OHProduct::sourceCount(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::sourceCount" << endl);
    data.addarg("Value", SoapHelp::i2s(o_sources.size()));
    return UPNP_E_SUCCESS;
}

int OHProduct::sourceXML(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::sourceXML" << endl);
    data.addarg("Value", csxml);
    return UPNP_E_SUCCESS;
}

int OHProduct::sourceIndex(const SoapIncoming& sc, SoapOutgoing& data)
{
    data.addarg("Value", SoapHelp::i2s(m_sourceIndex));
    LOGDEB("OHProduct::sourceIndex: " << m_sourceIndex << endl);
    return UPNP_E_SUCCESS;
}

int OHProduct::iSrcNameToIndex(const string& nm)
{
    for (unsigned int i = 0; i < o_sources.size(); i++) {
        if (!nm.compare(o_sources[i].second)) {
            return int(i);
        }
    }
    return -1;
}

int OHProduct::iSetSourceIndex(int sindex)
{
    LOGDEB("OHProduct::iSetSourceIndex: current " << m_sourceIndex <<
           " new " << sindex << endl);
    if (sindex < 0 || sindex >= int(o_sources.size())) {
        LOGERR("OHProduct::setSourceIndex: bad index: " << sindex << endl);
        return UPNP_E_INVALID_PARAM;
    }
    if (m_sourceIndex != sindex) {

        const MpdStatus& mpds = m_dev->getMpdStatus();
        int savedms = mpds.songelapsedms;

        m_dev->m_ohif->setMetatext("");

        string curnm = o_sources[m_sourceIndex].second;
        if (m_dev->m_ohpl && !curnm.compare("Playlist")) {
            m_dev->m_ohpl->iStop();
            m_dev->m_ohpl->setActive(false);
        } else if (m_dev->m_ohrcv && !curnm.compare("Receiver")) {
            m_dev->m_ohrcv->iStop();
            m_dev->m_ohrcv->setActive(false);
        } else if (m_dev->m_ohrd && !curnm.compare("Radio")) {
            m_dev->m_ohrd->iStop();
            m_dev->m_ohrd->setActive(false);
        } else if (m_dev->m_sndrcv && m_dev->m_ohpl &&
                   !curnm.compare("SenderReceiverPL")) {
            m_dev->m_sndrcv->stop();
            m_dev->m_ohpl->setActive(false);
        } else if (m_dev->m_sndrcv && m_dev->m_ohrd &&
                   !curnm.compare("SenderReceiverRD")) {
            m_dev->m_ohrd->setActive(false);
            m_dev->m_sndrcv->stop();
        }

        string newnm = o_sources[sindex].second;
        if (m_dev->m_ohpl && !newnm.compare("Playlist")) {
            m_dev->m_ohpl->setActive(true);
            m_sourceIndex = sindex;
        } else if (m_dev->m_ohrcv && !newnm.compare("Receiver")) {
            m_dev->m_ohrcv->setActive(true);
            m_sourceIndex = sindex;
        } else if (m_dev->m_ohrd && !newnm.compare("Radio")) {
            m_dev->m_ohrd->setActive(true);
            m_sourceIndex = sindex;
        } else if (m_dev->m_ohpl && m_dev->m_sndrcv &&
                   !newnm.compare("SenderReceiverPL")) {
            // Events are generated by playlist
            m_dev->m_ohpl->setActive(true);
            m_dev->m_sndrcv->start(false, savedms);
            m_sourceIndex = sindex;
        } else if (m_dev->m_ohrd && m_dev->m_sndrcv &&
                   !newnm.compare("SenderReceiverRD")) {
            // Events are generated by radio
            m_dev->m_ohrd->setActive(true);
            m_dev->m_sndrcv->start(true, 0);
            m_sourceIndex = sindex;
        }

        m_dev->loopWakeup();
    }
    return UPNP_E_SUCCESS;
}

int OHProduct::setSourceIndex(const SoapIncoming& sc, SoapOutgoing&)
{
    LOGDEB("OHProduct::setSourceIndex" << endl);
    int sindex;
    if (!sc.get("Value", &sindex)) {
        return UPNP_E_INVALID_PARAM;
    }
    return iSetSourceIndex(sindex);
}

int OHProduct::iSetSourceIndexByName(const string& name)
{
    LOGDEB("OHProduct::iSetSourceIndexByName: " << name << endl);
    int i = iSrcNameToIndex(name);
    if (i >= 0) {
        return iSetSourceIndex(i);
    } 
    LOGERR("OHProduct::iSetSourceIndexByName: no such name: " << name << endl);
    return UPNP_E_INVALID_PARAM;
}

int OHProduct::setSourceIndexByName(const SoapIncoming& sc, SoapOutgoing& data)
{

    string name;
    if (!sc.get("Value", &name)) {
        LOGERR("OHProduct::setSourceIndexByName: no Value" << endl);
        return UPNP_E_INVALID_PARAM;
    }
    return iSetSourceIndexByName(name);
}

int OHProduct::source(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::source" << endl);
    int sindex;
    if (!sc.get("Index", &sindex)) {
        return UPNP_E_INVALID_PARAM;
    }
    LOGDEB("OHProduct::source: " << sindex << endl);
    if (sindex < 0 || sindex >= int(o_sources.size())) {
        LOGERR("OHProduct::source: bad index: " << sindex << endl);
        return UPNP_E_INVALID_PARAM;
    }
    data.addarg("SystemName", m_roomOrName);
    data.addarg("Type", o_sources[sindex].first);
    data.addarg("Name", o_sources[sindex].second);
    string visible = o_sources[sindex].first.compare("Receiver") ? "1" : "0";
    data.addarg("Visible", visible);
    return UPNP_E_SUCCESS;
}

int OHProduct::attributes(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::attributes" << endl);
    data.addarg("Value", csattrs);
    return UPNP_E_SUCCESS;
}

int OHProduct::sourceXMLChangeCount(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHProduct::sourceXMLChangeCount" << endl);
    data.addarg("Value", "0");
    return UPNP_E_SUCCESS;
}
