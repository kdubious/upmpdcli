/* Copyright (C) 2018 J.F.Dockes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define LOGGER_LOCAL_LOGINC 6

#include "ohcredentials.hxx"

#include <upnp/upnp.h>
#include <sys/stat.h>

#include <functional>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <regex>

#include "conftree.h"
#include "main.hxx"
#include "pathut.h"
#include "execmd.h"
#include "mediaserver/cdplugins/cmdtalk.h"

#include "libupnpp/log.hxx"
#include "libupnpp/base64.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device/device.hxx"
#include "mediaserver/cdplugins/cdplugin.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpCredentials("urn:av-openhome-org:service:Credentials:1");
static const string sIdCredentials("urn:av-openhome-org:serviceId:Credentials");

static const string idstring{"tidalhifi.com qobuz.com"};
static const map<string, string> idmap {
    {"tidalhifi.com", "tidal"},
    {"qobuz.com", "qobuz"}
};

// This is used for translating urls for the special use of
// Kazoo/Lumin. The media server, which is used to run the http server
// and for getting the real media URLs, must run on this host (for one
// thing the creds are passed through a local file).
// *** Note that this needs xxxautostart to work, else the HTTP server
//     won't be listening (as long as nobody accesses the app section
//     of the media server) ***
static string upnphost;

// Called from OHPlaylist. The CP (Kazoo/Lumin mostly) will send URIs
// like qobuz:// tidal:// and expect the renderer to know what to do
// with them. We transform them so that they point to our media server
// gateway (which should be running of course for this to work).
bool OHCredsMaybeMorphSpecialUri(string& uri, bool& isStreaming)
{
    isStreaming = false;
    if (uri.find("http://") == 0 || uri.find("https://") == 0) {
        return true;
    }

    // Possibly retrieve the IP port used by our proxy server
    static string sport;
    if (sport.empty()) {
        std::unique_lock<std::mutex>(g_configlock);
        int port = CDPluginServices::default_microhttpport();
        if (!g_config->get("plgmicrohttpport", sport)) {
            sport = SoapHelp::i2s(port);
        }
    }

    // http://wiki.openhome.org/wiki/Av:Developer:Eriskay:StreamingServices
    // Tidal and qobuz tracks added by Kazoo / Lumin: 
    //   tidal://track?version=1&trackId=[tidal_track_id]
    //   qobuz://track?version=2&trackId=[qobuz_track_id]
    
    string se =
        "(tidal|qobuz)://track\\?version=([[:digit:]]+)&trackId=([[:digit:]]+)";
    std::regex e(se);
    std::smatch mr;
    bool found = std::regex_match(uri, mr, e);
    if (found) {
        string pathprefix = CDPluginServices::getpathprefix(mr[1]);

        // The microhttpd code actually only cares about getting a
        // trackId parameter. Make it look what the plugins normally
        // generate anyway:
        string path = path_cat(pathprefix,
                               "track?version=1&trackId=" + mr[3].str());
        uri = string("http://") + upnphost + ":" + sport + path;
        isStreaming = true;
    }
    return found;
}

// We might want to derive this into ServiceCredsQobuz,
// ServiceCredsTidal, there is a lot in common and a few diffs.
struct ServiceCreds {
    ServiceCreds() {}
    ServiceCreds(const string& inm, const string& u, const string& p,
                 const string& ep)
        : servicename(inm), user(u), password(p), encryptedpass(ep) {

        if (servicename == "qobuz") {
            // The appid used by the Qobuz python module. Has to be
            // consistent with the token obtained by the same, so we
            // return it (by convention, as seen in wiresharking
            // kazoo) in the data field. We could and do obtain the
            // appid from the module, but kazoo apparently wants it
            // before we login, so just hard-code it for now.  The
            // Python code uses the value from XBMC,285473059,
            // ohplayer uses 854233864
            data = "285473059";
        } else if (servicename == "tidal") {
            // data contains the country code
            data = "FR";
        }
    }

    ~ServiceCreds() {
        delete cmd;
    }

    // We need a Python helper to perform the login. That's the media
    // server gateway module, from which we only use a separate method
    // which logs-in and returns the auth data (token, etc.)
    bool maybeStartCmd() {
        LOGDEB("ServiceCreds: " << servicename << " maybeStartCmd()\n");
        if (nullptr == cmd) {
            cmd = new CmdTalk(30);
        }
        if (cmd->running()) {
            return true;
        }
        LOGDEB("ServiceCreds: " << servicename << " starting cmd\n");
        string exepath = path_cat(g_datadir, "cdplugins");
        exepath = path_cat(exepath, servicename);
        exepath = path_cat(exepath, servicename + "-app" + ".py");

        string pythonpath = string("PYTHONPATH=") +
            path_cat(g_datadir, "cdplugins") + ":" +
            path_cat(g_datadir, "cdplugins/pycommon") + ":" +
            path_cat(g_datadir, "cdplugins/" + servicename);
        string configname = string("UPMPD_CONFIG=") + g_configfilename;
        // hostport is not needed by this login-only instance.
        string hostport = string("UPMPD_HTTPHOSTPORT=bogus:0");
        string pp = string("UPMPD_PATHPREFIX=") +
            CDPluginServices::getpathprefix(servicename);
        if (!cmd->startCmd(exepath, {/*args*/},
                           /* env */ {pythonpath, configname, hostport, pp})) {
            LOGERR("ServiceCreds::maybeStartCmd: startCmd failed\n");
            return false;
        }
        LOGDEB("ServiceCreds: " << servicename << " cmd started\n");
        return true;
    }

    string login() {
        LOGDEB("ServiceCreds: " << servicename << " login\n");

        // Check if already logged-in
        if (servicename == "qobuz" || servicename == "tidal") {
            if (!servicedata["token"].empty()) {
                return servicedata["token"];
            }
        } else {
            LOGERR("Unsupported service: " << servicename << endl);
            return string();
        }

        if (!maybeStartCmd()) {
            return string();
        }
        unordered_map<string,string> res;
        if (!cmd->callproc("login", {{"user", user},
                    {"password", password}}, res)) {
            LOGERR("ServiceCreds::login: slave failure. Service " <<
                   servicename << " user " << user << endl);
            return string();
        }

        vector<string> toknames;
        if (servicename == "qobuz") {
            toknames = vector<string>{"token", "appid"};
        } else if (servicename == "tidal") {
            toknames = vector<string>{"token", "country"};
        }
        for (const auto& toknm : toknames) {
            auto it = res.find(toknm);
            if (it == res.end()) {
                LOGERR("ServiceCreds::login: no " << toknm << ". Service " <<
                       servicename << " user " << user << endl);
                return string();
            }
            servicedata[toknm] = it->second;
        }
        if (servicename == "qobuz") {
            data = servicedata["appid"];
        } else if (servicename == "tidal") {
            data = servicedata["country"];
        }
        return servicedata["token"];
    }

    void logout() {
        servicedata.clear();
    }

    string str() {
        string s;
        string sdata;
        for (const auto& entry:servicedata) {
            sdata += entry.first + " : " + entry.second + ", ";
        }
        s += "Service: " + servicename + " User: " + user +
            /*" Pass: "+password*/ + " Servicedata: " + sdata +
            /*" EPass: "+encryptedpass*/ + " Enabled: " +
            SoapHelp::i2s(enabled) + " Status: " + status + " Data: " + data;
        return s;
    }

    // Internal name, like "qobuz"
    string servicename;
    string user;
    string password;
    string encryptedpass;
    bool enabled{true};
    CmdTalk *cmd{0};
    // Things we obtain from the module and send to the CP
    unordered_map<string,string> servicedata;

    string status;
    // See comments about 'data' use above.
    string data;
};

class OHCredentials::Internal {
public:
    
    Internal(const string& cd) {
        cachedir = path_cat(cd, "ohcreds");
        if (!path_makepath(cachedir, 0700)) {
            LOGERR("OHCredentials: can't create cache dir " << cachedir <<endl);
            return;
        }
        keyfile = path_cat(cachedir, "credkey.pem");
        cmd.putenv("RANDFILE", path_cat(cachedir, "randfile"));

        if (!path_exists(keyfile)) {
            vector<string> acmd{"openssl", "genrsa", "-out", keyfile, "4096"};
            int status = cmd.doexec1(acmd);
            chmod(keyfile.c_str(), 0600);
            if (status != 0) {
                LOGERR("OHCredentials: could not create key\n");
                return;
            }
        }

        vector<string> acmd{"openssl", "pkey", "-in", keyfile, "-pubout"};
        if (!cmd.backtick(acmd, pubkey)) {
            LOGERR("OHCredentials: could not read public key\n");
            return;
        }

        LOGDEB1("OHCredentials: my public key:\n" << pubkey << endl);
    }

    bool decrypt(const string& in, string& out) {
        vector<string> acmd{"openssl", "pkeyutl", "-inkey",
                keyfile, "-pkeyopt", "rsa_padding_mode:oaep", "-decrypt"};
        int status = cmd.doexec1(acmd, &in, &out);
        if (status) {
            LOGERR("OHCredentials: decrypt failed\n");
            return false;
        }
        //LOGDEB1("decrypt: [" << out << "]\n");
        return true;
    }

    bool setEnabled(const string& id, bool enabled) {
        auto it = creds.find(id);
        if (it == creds.end()) {
            return false;
        }
        it->second.enabled = enabled;
        return true;
    }

    bool save() {
        string credsfile = path_cat(cachedir, "screds");
        ConfSimple credsconf(credsfile.c_str());
        if (!credsconf.ok()) {
            LOGERR("OHCredentials: error opening " << credsfile <<
                   " errno " << errno << endl);
            return false;
        }
        for (const auto& cred : creds) {
            credsconf.set("u", cred.second.user, cred.second.servicename);
            credsconf.set("p", cred.second.password, cred.second.servicename);
        }
        chmod(credsfile.c_str(), 0600);
        return true;
    }
    
    ExecCmd cmd;
    string cachedir;
    string keyfile;
    string pubkey;
    int seq{1};
    map<string, ServiceCreds> creds;
};


OHCredentials::OHCredentials(UpMpd *dev, const string& cachedir)
    : OHService(sTpCredentials, sIdCredentials, dev), m(new Internal(cachedir))
{
    dev->addActionMapping(
        this, "Set",
        bind(&OHCredentials::actSet, this, _1, _2));
    dev->addActionMapping(
        this, "Clear",
        bind(&OHCredentials::actClear, this, _1, _2));
    dev->addActionMapping(
        this, "SetEnabled",
        bind(&OHCredentials::actSetEnabled, this, _1, _2));
    dev->addActionMapping(
        this, "Get",
        bind(&OHCredentials::actGet, this, _1, _2));
    dev->addActionMapping(
        this, "Login",
        bind(&OHCredentials::actLogin, this, _1, _2));
    dev->addActionMapping(
        this, "ReLogin",
        bind(&OHCredentials::actReLogin, this, _1, _2));
    dev->addActionMapping(
        this, "GetIds",
        bind(&OHCredentials::actGetIds, this, _1, _2));
    dev->addActionMapping(
        this, "GetPublicKey",
        bind(&OHCredentials::actGetPublicKey, this, _1, _2));
    dev->addActionMapping(
        this, "GetSequenceNumber",
        bind(&OHCredentials::actGetSequenceNumber, this, _1, _2));

    unsigned short usport;
    dev->ipv4(&upnphost, &usport);
}

OHCredentials::~OHCredentials()
{
    delete m;
}

bool OHCredentials::makestate(unordered_map<string, string> &st)
{
    st.clear();
    if (nullptr == m) {
        return false;
    }
    st["Ids"] = idstring;
    st["PublicKey"] = m->pubkey;
    st["SequenceNumber"] = SoapHelp::i2s(m->seq);
    return true;
}

int OHCredentials::actSet(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actSet: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_UserName;
    ok = sc.get("UserName", &in_UserName);
    if (!ok) {
        LOGERR("OHCredentials::actSet: no UserName in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    string in_Password;
    ok = sc.get("Password", &in_Password);
    if (!ok) {
        LOGERR("OHCredentials::actSet: no Password in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actSet: " << " Id " << in_Id << " UserName " <<
           in_UserName << " Password " << in_Password << endl);

    const auto it1 = idmap.find(in_Id);
    if (it1 == idmap.end()) {
        LOGERR("OHCredentials::actSet: bad service id [" << in_Id <<"]\n");
        return UPNP_E_INVALID_PARAM;
    }
    string servicename = it1->second;
    string cpass = base64_decode(in_Password);
    string plainpass;
    if (!m->decrypt(cpass, plainpass)) {
        LOGERR("OHCredentials::actSet: could not decrypt\n");
        return UPNP_E_INVALID_PARAM;
    }
    auto it = m->creds.find(in_Id);
    if (it == m->creds.end() || it->second.user != in_UserName ||
        it->second.password != plainpass ||
        it->second.encryptedpass != in_Password) {
        m->creds[in_Id] = ServiceCreds(servicename, in_UserName, plainpass,
                                       in_Password);
    }
    m->seq++;
    m->save();
    if (m->setEnabled(in_Id, true)) {
        return UPNP_E_SUCCESS;
    } else {
        return UPNP_E_INVALID_PARAM;
    }
}

int OHCredentials::actLogin(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actLogin: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actLogin: " << " Id " << in_Id << endl);
    auto it = m->creds.find(in_Id);
    if (it == m->creds.end()) {
        LOGERR("OHCredentials::Login: Id " << in_Id << " not found\n");
        return UPNP_E_INVALID_PARAM;
    }
    string token = it->second.login();
    LOGDEB("OHCredentials::Login: got token " << token << endl);
    data.addarg("Token", token);
    m->seq++;
    return UPNP_E_SUCCESS;
}

int OHCredentials::actReLogin(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actReLogin: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    std::string in_CurrentToken;
    ok = sc.get("CurrentToken", &in_CurrentToken);
    if (!ok) {
        LOGERR("OHCredentials::actReLogin: no CurrentToken in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actReLogin: " << " Id " << in_Id << " CurrentToken "
           << in_CurrentToken << endl);

    auto it = m->creds.find(in_Id);
    if (it == m->creds.end()) {
        LOGERR("OHCredentials::Login: Id " << in_Id << " not found\n");
        return UPNP_E_INVALID_PARAM;
    }
    it->second.logout();
    string token = it->second.login();
    data.addarg("NewToken", token);
    m->seq++;
    return UPNP_E_SUCCESS;
}

int OHCredentials::actClear(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actClear: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actClear: " << " Id " << in_Id << endl);
    if (idmap.find(in_Id) == idmap.end()) {
        LOGERR("OHCredentials::actClear: bad service id [" << in_Id <<"]\n");
        return UPNP_E_INVALID_PARAM;
    }
    auto it = m->creds.find(in_Id);
    if (it != m->creds.end()) {
        m->creds.erase(it);
        m->seq++;
    }
    return UPNP_E_SUCCESS;
}

int OHCredentials::actSetEnabled(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actSetEnabled: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }
    bool in_Enabled;
    ok = sc.get("Enabled", &in_Enabled);
    if (!ok) {
        LOGERR("OHCredentials::actSetEnabled: no Enabled in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actSetEnabled: " << " Id " << in_Id << " Enabled " <<
           in_Enabled << endl);
    if (m->setEnabled(in_Id, in_Enabled)) {
        m->seq++;
        return UPNP_E_SUCCESS;
    } else {
        return UPNP_E_INVALID_PARAM;
    }
}

int OHCredentials::actGet(const SoapIncoming& sc, SoapOutgoing& data)
{
    bool ok = false;
    std::string in_Id;
    ok = sc.get("Id", &in_Id);
    if (!ok) {
        LOGERR("OHCredentials::actGet: no Id in params\n");
        return UPNP_E_INVALID_PARAM;
    }

    LOGDEB("OHCredentials::actGet: " << " Id " << in_Id << endl);

    auto it = m->creds.find(in_Id);
    ServiceCreds emptycreds;
    ServiceCreds *credsp(&emptycreds);
    if (it != m->creds.end()) {
        credsp = &(it->second);
    } else {
        LOGDEB("OHCredentials::actGet: nothing found for " << in_Id << endl);
    }
    LOGDEB("OHCredentials::actGet: data for " << in_Id << " " <<
           credsp->str() << endl);
    data.addarg("UserName", credsp->user);
    // Encrypted password !
    data.addarg("Password", credsp->encryptedpass);
    // In theory enabled is set in response to setEnabled() or
    // set(). In practise, if it is not set, we don't get to the qobuz
    // settings screen in kazoo.
    data.addarg("Enabled", credsp->enabled ? "1" : "1");
    data.addarg("Status", credsp->status);
    data.addarg("Data", credsp->data);
    return UPNP_E_SUCCESS;
}

int OHCredentials::actGetIds(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHCredentials::actGetIds: " << endl);
    data.addarg("Ids", idstring);
    return UPNP_E_SUCCESS;
}

int OHCredentials::actGetPublicKey(const SoapIncoming& sc, SoapOutgoing& data)
{
    LOGDEB("OHCredentials::actGetPublicKey: " << endl);
    data.addarg("PublicKey", m->pubkey);
    return m->pubkey.empty() ? UPNP_E_INTERNAL_ERROR : UPNP_E_SUCCESS;
}

int OHCredentials::actGetSequenceNumber(const SoapIncoming& sc,
                                        SoapOutgoing& data)
{
    LOGDEB("OHCredentials::actGetSequenceNumber: " << endl);
    data.addarg("SequenceNumber", SoapHelp::i2s(m->seq));
    return UPNP_E_SUCCESS;
}
