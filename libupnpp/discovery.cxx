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
using namespace std;

#include "upnpp_p.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include "workqueue.hxx"
#include "expatmm.hxx"
#include "upnpplib.hxx"
#include "description.hxx"
#include "cdirectory.hxx"
#include "discovery.hxx"
#include "log.hxx"

#undef LOCAL_LOGINC
#define LOCAL_LOGINC 0

// The service type string for Content Directories:
static const string
ContentDirectorySType("urn:schemas-upnp-org:service:ContentDirectory:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
static bool isCDService(const string& st)
{
    const string::size_type sz(ContentDirectorySType.size()-2);
    return !ContentDirectorySType.compare(0, sz, st, 0, sz);
}

// The device type string for Media Servers
static const string
MediaServerDType("urn:schemas-upnp-org:device:MediaServer:1") ;

#if 0
static bool isMSDevice(const string& st)
{
    const string::size_type sz(MediaServerDType.size()-2);
    return !MediaServerDType.compare(0, sz, st, 0, sz);
}
#endif

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

// Descriptor for one device having a Content Directory service found
// on the network.
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
        PLOGDEB("discoExplorer: alive %d deviceId [%s] URL [%s]\n",
                tsk->alive, tsk->deviceId.c_str(), tsk->url.c_str());
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
                        
            //LOGDEB("discoExplorer: downloaded description document of " <<
            //   sdesc.size() << " bytes" << endl);

            // Update or insert the device
            DeviceDescriptor d(tsk->url, sdesc, time(0), tsk->expires);
            if (!d.device.ok) {
                LOGERR("discoExplorer: description parse failed for " << 
                       tsk->deviceId << endl);
                delete tsk;
                continue;
            }
            PTMutexLocker lock(o_pool.m_mutex);
            //LOGDEB("discoExplorer: inserting device id "<< tsk->deviceId << 
            //       " description: " << endl << d.device.dump() << endl);
            o_pool.m_devices[tsk->deviceId] = d;
        }
        delete tsk;
    }
}

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
    //LOGDEB("discovery:cluCallBack: " << LibUPnP::evTypeAsString(et) << endl);

    switch (et) {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        struct Upnp_Discovery *disco = (struct Upnp_Discovery *)evp;
        //LOGDEB("discovery:cllb:ALIVE: " << cluDiscoveryToStr(disco) << endl);
        DiscoveredTask *tp = new DiscoveredTask(1, disco);
        if (discoveredQueue.put(tp)) {
            return UPNP_E_FINISH;
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
    PLOGDEB("UPnPDeviceDirectory::search\n");
    if (time(0) - m_lastSearch < 10)
        return true;

    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        m_reason = "Can't get lib";
        return false;
    }

    // We search both for device and service just in case.
    int code1 = UpnpSearchAsync(lib->getclh(), m_searchTimeout,
                                ContentDirectorySType.c_str(), lib);
    if (code1 != UPNP_E_SUCCESS) {
        m_reason = LibUPnP::errAsString("UpnpSearchAsync", code1);
    }
    int code2 = UpnpSearchAsync(lib->getclh(), m_searchTimeout,
                                MediaServerDType.c_str(), lib);
    if (code2 != UPNP_E_SUCCESS) {
        m_reason = LibUPnP::errAsString("UpnpSearchAsync", code2);
    }
    if (code1 != UPNP_E_SUCCESS && code2 != UPNP_E_SUCCESS)
        return false;
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

bool UPnPDeviceDirectory::getDirServices(vector<ContentDirectoryService>& out)
{
    //LOGDEB("UPnPDeviceDirectory::getDirServices" << endl);
    if (m_ok == false)
        return false;

    if (getRemainingDelay() > 0)
        sleep(getRemainingDelay());

    // Has locking, do it before our own lock
    expireDevices();

    PTMutexLocker lock(o_pool.m_mutex);

    for (DevPoolIt dit = o_pool.m_devices.begin();
         dit != o_pool.m_devices.end(); dit++) {
        for (DevServIt sit = dit->second.device.services.begin();
             sit != dit->second.device.services.end(); sit++) {
            if (isCDService(sit->serviceType)) {
                out.push_back(ContentDirectoryService(dit->second.device,
                                                      *sit));
            }
        }
    }

    return true;
}

// Get server by friendly name. It's a bit wasteful to copy all
// servers for this, we could directly walk the list. Otoh there isn't
// going to be millions...
bool UPnPDeviceDirectory::getServer(const string& friendlyName,
                                    ContentDirectoryService& server)
{
    vector<ContentDirectoryService> ds;
    if (!getDirServices(ds)) {
        PLOGDEB("UPnPDeviceDirectory::getServer: no servers?\n");
        return false;
    }
    for (vector<ContentDirectoryService>::const_iterator it = ds.begin();
         it != ds.end(); it++) {
        if (!friendlyName.compare(it->getFriendlyName())) {
            server = *it;
            return true;
        }
    }
    return false;
}

