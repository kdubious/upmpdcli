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

#include <functional>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "conftree.h"
#include "main.hxx"
#include "pathut.h"
#include "execmd.h"
#include "mediaserver/cdplugins/cmdtalk.h"

#include "libupnpp/log.hxx"
#include "libupnpp/base64.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device/device.hxx"

using namespace std;
using namespace std::placeholders;

static const string sTpCredentials("urn:av-openhome-org:service:Credentials:1");
static const string sIdCredentials("urn:av-openhome-org:serviceId:Credentials");

static const string idstring{"tidalhifi.com qobuz.com"};
static const map<string, string> idmap {
    {"tidalhifi.com", "tidal"},
    {"qobuz.com", "qobuz"}
};

// Should be derived into ServiceCredsQobuz, ServiceCredsTidal, there
// is a lot in common and a few diffs.
struct ServiceCreds {
    ServiceCreds() {}
    ServiceCreds(const string& inm, const string& u, const string& p,
                 const string& ep)
        : servicename(inm), user(u), password(p), encryptedpass(ep) {
        // The appid used by the Qobuz python module. Has to be
        // consistent with the token obtained by the same, so we
        // return it (by convention, as seen in wiresharking kazoo) in
        // the data field. Of course, this class should be derived
        // into service-specific ones, and this should happen only for
        // the qobuz version. We could and do obtain the appid from
        // the module, but kazoo wants it before we login, so just
        // hard-code it for now.
        data = "285473059";
    }
    ~ServiceCreds() {
        delete cmd;
    }

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
        if (!cmd->startCmd(exepath, {/*args*/},
                          /* env */ {pythonpath})) {
            LOGERR("ServiceCreds::maybeStartCmd: startCmd failed\n");
            return false;
        }
        LOGDEB("ServiceCreds: " << servicename << " cmd started\n");
        return true;
    }

    string login() {
        LOGDEB("ServiceCreds: " << servicename << " login\n");
        if (!token.empty()) {
            return token;
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
        auto it = res.find("appid");
        if (it == res.end()) {
            LOGERR("ServiceCreds::login: no appid. Service " <<
                   servicename << " user " << user << endl);
            return string();
        }
        appid = it->second;
        data = appid;
        it = res.find("token");
        if (it == res.end()) {
            LOGERR("ServiceCreds::login: no token. Service " <<
                   servicename << " user " << user << endl);
            return string();
        }
        return token = it->second;
    }

    void logout() {
        appid.clear();
        token.clear();
    }

    string str() {
        string s;
        s += "Service: " + servicename + " User: " + user +
            " Pass: " + password + " Appid " + appid + " Token " + token +
            " EPass: " + encryptedpass + " Enabled: " +
            SoapHelp::i2s(enabled) + " Status: " + status + " Data: " + data;
        return s;
    }

    // Internal name, like "qobuz"
    string servicename;
    string user;
    string password;
    string encryptedpass;
    string appid;
    string token;
    bool enabled{true};
    string status;
    // For qobuz, data contains the "app id" 854233864 for ohplayer.
    // XBMC is 285473059
    string data;
    CmdTalk *cmd{0};
};

class OHCredentials::Internal {
public:
    
    Internal(const string& cd) {
        string cachedir = path_cat(cd, "ohcreds");
        if (!path_makepath(cachedir, 0700)) {
            LOGERR("OHCredentials: can't create cache dir " << cachedir <<endl);
            return;
        }
        keyfile = path_cat(cachedir, "credkey.pem");
        cmd.putenv("RANDFILE", path_cat(cachedir, "randfile"));

        if (!path_exists(keyfile)) {
            vector<string> acmd{"openssl", "genrsa", "-out", keyfile, "4096"};
            int status = cmd.doexec1(acmd);
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

        LOGDEB("OHCredentials: my public key:\n" << pubkey << endl);
    }

    bool decrypt(const string& in, string& out) {
        vector<string> acmd{"openssl", "pkeyutl", "-inkey",
                keyfile, "-pkeyopt", "rsa_padding_mode:oaep", "-decrypt"};
        int status = cmd.doexec1(acmd, &in, &out);
        if (status) {
            LOGERR("OHCredentials: decrypt failed\n");
            return false;
        }
        LOGDEB("decrypt: [" << out << "]\n");
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
    
    ExecCmd cmd;
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
