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
#ifndef _SERVICE_H_X_INCLUDED_
#define _SERVICE_H_X_INCLUDED_

#include <string>
#include <functional>
#include <unordered_map>

#include <upnp/upnp.h>

#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/control/description.hxx"
#include "libupnpp/control/cdircontent.hxx"


namespace UPnPClient {

/** To be implemented by upper-level client code for event
 * reporting. Runs in an event thread. This could for example be
 * implemented by a Qt Object to generate events for the GUI.
 */
class VarEventReporter {
public:
    virtual ~VarEventReporter() {};
    // Using char * to avoid any issue with strings and concurrency
    virtual void changed(const char *nm, int val)  = 0;
    virtual void changed(const char *nm, const char *val) = 0;
    // Used for track metadata (parsed as content directory entry). Not always
    // needed.
    virtual void changed(const char *nm, UPnPDirContent meta) {};
};

typedef 
std::function<void (const std::unordered_map<std::string, std::string>&)> 
evtCBFunc;

class Service {
public:
    /** Construct by copying data from device and service objects.
     */
    Service(const UPnPDeviceDesc& device,
            const UPnPServiceDesc& service)
        : m_reporter(0), 
          m_actionURL(caturl(device.URLBase, service.controlURL)),
          m_eventURL(caturl(device.URLBase, service.eventSubURL)),
          m_serviceType(service.serviceType),
          m_deviceId(device.UDN),
          m_friendlyName(device.friendlyName),
          m_manufacturer(device.manufacturer),
          m_modelName(device.modelName)
    { 
        initEvents();
        subscribe();
    }

    /** An empty one */
    Service() : m_reporter(0) {}

    virtual ~Service() 
    {
        o_calls.erase(m_SID);
    }

    /** Retrieve my root device "friendly name". */
    std::string getFriendlyName() const {return m_friendlyName;}

    /** Return my root device id */
    std::string getDeviceId() const {return m_deviceId;}

    virtual int runAction(const SoapEncodeInput& args, SoapDecodeOutput& data);

    virtual VarEventReporter *getReporter()
    {
        return m_reporter;
    }

    virtual void installReporter(VarEventReporter* reporter)
    {
        m_reporter = reporter;
        LOGDEB("Reporter now " << m_reporter << endl);
    }

    // Can't copy these because this does not make sense for the
    // member function callback.
    Service(Service const&) = delete;
    Service& operator=(Service const&) = delete;

protected:

    /** Registered callbacks for the service objects. The map is
     * indexed by m_SID, the subscription id which was obtained by
     * each object when subscribing to receive the events for its
     * device. The map allows the static function registered with
     * libupnp to call the appropriate object method when it receives
     * an event. */
    static std::unordered_map<std::string, evtCBFunc> o_calls;

    /** Used by a derived class to register its callback method. This
     * creates an entry in the static map, using m_SID, which was
     * obtained by subscribe() during construction 
     */
    void registerCallback(evtCBFunc c);

    /** Upper level client code event callbacks */
    VarEventReporter *m_reporter;

    std::string m_actionURL;
    std::string m_eventURL;
    std::string m_serviceType;
    std::string m_deviceId;
    std::string m_friendlyName;
    std::string m_manufacturer;
    std::string m_modelName;

private:
    /** Only actually does something on the first call, to register our
     * (static) library callback */
    static bool initEvents();
    /** The static event callback given to libupnp */
    static int srvCB(Upnp_EventType et, void* vevp, void*);
    /* Tell the UPnP device (through libupnp) that we want to receive
       its events. This is called during construction and sets m_SID */
    virtual bool subscribe();

    Upnp_SID    m_SID; /* Subscription Id */
};

} // namespace UPnPClient

#endif /* _SERVICE_H_X_INCLUDED_ */
