/* Copyright (C) 2013 J.F.Dockes
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
#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <functional>
using namespace std;
using namespace std::placeholders;

#include "upnpp_p.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include "workqueue.hxx"
#include "upnpplib.hxx"
#include "description.hxx"
#include "discovery.hxx"
#include "log.hxx"

namespace UPnPClient {

//#undef LOCAL_LOGINC
//#define LOCAL_LOGINC 3

static string cluDiscoveryToStr(const struct Upnp_Discovery *disco)
{
    stringstream ss;
    ss << "ErrCode: " << disco->ErrCode << endl;
    ss << "Expires: " << disco->Expires << endl;
    ss << "DeviceId: " << disco->DeviceId << endl;
    ss << "DeviceType: " << disco->DeviceType << endl;
    ss << "ServiceType: " << disco->ServiceType << endl;
    ss << "ServiceVer: " << disco->ServiceVer    << endl;
    ss << "Location: " << disco->Location << endl;
    ss << "Os: " << disco->Os << endl;
    ss << "Date: " << disco->Date << endl;
    ss << "Ext: " << disco->Ext << endl;

    /** The host address of the device responding to the search. */
    // struct sockaddr_storage DestAddr;
    return ss.str();
}

// Each appropriate discovery event (executing in a libupnp thread
// context) queues the following task object for processing by the
// discovery thread.
class DiscoveredTask {
public:
    DiscoveredTask(bool _alive, const struct Upnp_Discovery *disco)
        : alive(_alive), url(disco->Location), deviceId(disco->DeviceId),
          expires(disco->Expires)
        {}

    bool alive;
    string url;
    string deviceId;
    int expires; // Seconds valid
};
static WorkQueue<DiscoveredTask*> discoveredQueue("DiscoveredQueue");

// This gets called in a libupnp thread context for all asynchronous
// events which we asked for.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
// It seems that this can get called by several threads. We have a
// mutex just for clarifying the message printing, the workqueue is
// mt-safe of course.
static PTMutexInit cblock;
static int cluCallBack(Upnp_EventType et, void* evp, void*)
{
    PTMutexLocker lock(cblock);
    LOGDEB1("discovery:cluCallBack: " << LibUPnP::evTypeAsString(et) << endl);

    switch (et) {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        struct Upnp_Discovery *disco = (struct Upnp_Discovery *)evp;
        // Devices send multiple messages for themselves, their subdevices and 
        // services. AFAIK they all point to the same description.xml document,
        // which has all the interesting data. So let's try to only process
        // one message per device: the one which probably correspond to the 
        // upnp "root device" message and has empty service and device types:
        if (!disco->DeviceType[0] && !disco->ServiceType[0]) {
            LOGDEB1("discovery:cllb:ALIVE: " << cluDiscoveryToStr(disco) 
                    << endl);
            DiscoveredTask *tp = new DiscoveredTask(1, disco);
            if (discoveredQueue.put(tp)) {
                return UPNP_E_FINISH;
            }
        }
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery *disco = (struct Upnp_Discovery *)evp;
        //LOGDEB("discovery:cllB:BYEBYE: " << cluDiscoveryToStr(disco) << endl);
        DiscoveredTask *tp = new DiscoveredTask(0, disco);
        if (discoveredQueue.put(tp)) {
            return UPNP_E_FINISH;
        }
        break;
    }
    default:
        // Ignore other events for now
        LOGDEB("discovery:cluCallBack: unprocessed evt type: [" << 
               LibUPnP::evTypeAsString(et) << "]"  << endl);
        break;
    }

    return UPNP_E_SUCCESS;
}

// Descriptor for one device found on the network.
class DeviceDescriptor {
public:
    DeviceDescriptor(const string& url, const string& description,
                     time_t last, int exp)
        : device(url, description), last_seen(last), expires(exp+20)
        {}
    DeviceDescriptor()
        {}
    UPnPDeviceDesc device;
    time_t last_seen;
    int expires; // seconds valid
};

// A DevicePool holds the characteristics of the devices
// currently on the network.
// The map is referenced by deviceId (==UDN)
// The class is instanciated as a static (unenforced) singleton.
class DevicePool {
public:
    PTMutexInit m_mutex;
    map<string, DeviceDescriptor> m_devices;
};
static DevicePool o_pool;
typedef map<string, DeviceDescriptor>::iterator DevPoolIt;

// Worker routine for the discovery queue. Get messages about devices
// appearing and disappearing, and update the directory pool
// accordingly.
static void *discoExplorer(void *)
{
    for (;;) {
        DiscoveredTask *tsk = 0;
        size_t qsz;
        if (!discoveredQueue.take(&tsk, &qsz)) {
            discoveredQueue.workerExit();
            return (void*)1;
        }
        LOGDEB1("discoExplorer: got task: alive " << tsk->alive << " deviceId ["
                << tsk->deviceId << " URL [" << tsk->url << "]" << endl);

        if (!tsk->alive) {
            // Device signals it is going off.
            PTMutexLocker lock(o_pool.m_mutex);
            DevPoolIt it = o_pool.m_devices.find(tsk->deviceId);
            if (it != o_pool.m_devices.end()) {
                o_pool.m_devices.erase(it);
                //LOGDEB("discoExplorer: delete " << tsk->deviceId.c_str() << 
                // endl);
            }
        } else {
            // Device signals its existence and well-being. Perform the
            // UPnP "description" phase by downloading and decoding the
            // description document.
            char *buf = 0;
            // LINE_SIZE is defined by libupnp's upnp.h...
            char contentType[LINE_SIZE];
            int code = UpnpDownloadUrlItem(tsk->url.c_str(), &buf, contentType);
            if (code != UPNP_E_SUCCESS) {
                LOGERR(LibUPnP::errAsString("discoExplorer", code) << endl);
                continue;
            }
            string sdesc(buf);
            free(buf);
                        
            LOGDEB1("discoExplorer: downloaded description document of " <<
                    sdesc.size() << " bytes" << endl);

            // Update or insert the device
            DeviceDescriptor d(tsk->url, sdesc, time(0), tsk->expires);
            if (!d.device.ok) {
                LOGERR("discoExplorer: description parse failed for " << 
                       tsk->deviceId << endl);
                delete tsk;
                continue;
            }
            PTMutexLocker lock(o_pool.m_mutex);
            //LOGDEB1("discoExplorer: inserting device id "<< tsk->deviceId << 
            //        " description: " << endl << d.device.dump() << endl);
            o_pool.m_devices[tsk->deviceId] = d;
        }
        delete tsk;
    }
}

// Look at the devices and get rid of those which have not been seen
// for too long. We do this when listing the top directory
void UPnPDeviceDirectory::expireDevices()
{
    LOGDEB1("discovery: expireDevices:" << endl);
    PTMutexLocker lock(o_pool.m_mutex);
    time_t now = time(0);
    bool didsomething = false;

    for (DevPoolIt it = o_pool.m_devices.begin();
         it != o_pool.m_devices.end();) {
        LOGDEB1("Dev in pool: type: " << it->second.device.deviceType <<
                " friendlyName " << it->second.device.friendlyName << endl);
        if (now - it->second.last_seen > it->second.expires) {
            //LOGDEB("expireDevices: deleting " <<  it->first.c_str() << " " << 
            //   it->second.device.friendlyName.c_str() << endl);
            o_pool.m_devices.erase(it++);
            didsomething = true;
        } else {
            it++;
        }
    }
    if (didsomething)
        search();
}

// m_searchTimeout is the UPnP device search timeout, which should
// actually be called delay because it's the base of a random delay
// that the devices apply to avoid responding all at the same time.
// This means that you have to wait for the specified period before
// the results are complete.
UPnPDeviceDirectory::UPnPDeviceDirectory(time_t search_window)
    : m_ok(false), m_searchTimeout(search_window), m_lastSearch(0)
{
    if (!discoveredQueue.start(1, discoExplorer, 0)) {
        m_reason = "Discover work queue start failed";
        return;
    }
    pthread_yield();
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        m_reason = "Can't get lib";
        return;
    }
    lib->registerHandler(UPNP_DISCOVERY_SEARCH_RESULT, cluCallBack, this);
    lib->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_ALIVE,
                         cluCallBack, this);
    lib->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE,
                         cluCallBack, this);

    m_ok = search();
}

bool UPnPDeviceDirectory::search()
{
    LOGDEB1("UPnPDeviceDirectory::search" << endl);
    if (time(0) - m_lastSearch < 10)
        return true;

    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        m_reason = "Can't get lib";
        return false;
    }

    LOGDEB1("UPnPDeviceDirectory::search: calling upnpsearchasync"<<endl);
    //const char *cp = "ssdp:all";
    const char *cp = "upnp:rootdevice";
    int code1 = UpnpSearchAsync(lib->getclh(), m_searchTimeout, cp, lib);
    if (code1 != UPNP_E_SUCCESS) {
        m_reason = LibUPnP::errAsString("UpnpSearchAsync", code1);
        LOGERR("UPnPDeviceDirectory::search: UpnpSearchAsync failed: " <<
               m_reason << endl);
    }
    m_lastSearch = time(0);
    return true;
}

static UPnPDeviceDirectory *theDevDir;
UPnPDeviceDirectory *UPnPDeviceDirectory::getTheDir(time_t search_window)
{
    if (theDevDir == 0)
        theDevDir = new UPnPDeviceDirectory(search_window);
    if (theDevDir && !theDevDir->ok())
        return 0;
    return theDevDir;
}

void UPnPDeviceDirectory::terminate()
{
    discoveredQueue.setTerminateAndWait();
}

time_t UPnPDeviceDirectory::getRemainingDelay()
{
    time_t now = time(0);
    if (now - m_lastSearch >= m_searchTimeout)
        return 0;
    return  m_searchTimeout - (now - m_lastSearch);
}

bool UPnPDeviceDirectory::traverse(UPnPDeviceDirectory::Visitor visit)
{
    //LOGDEB("UPnPDeviceDirectory::traverse" << endl);
    if (m_ok == false)
        return false;

    if (getRemainingDelay() > 0)
        sleep(getRemainingDelay());

    // Has locking, do it before our own lock
    expireDevices();

    PTMutexLocker lock(o_pool.m_mutex);

    for (auto& dde : o_pool.m_devices) {
        for (auto& service : dde.second.device.services) {
            if (!visit(dde.second.device, service))
                return false;
        }
    }
    return true;
}

} // namespace UPnPClient

