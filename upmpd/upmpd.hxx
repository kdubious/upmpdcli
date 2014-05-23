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

class MPDCli;
class MpdStatus;

// The UPnP MPD frontend device with its 3 services
class UpMpd : public UpnpDevice {
public:
    friend class UpMpdRenderCtl;
    friend class UpMpdAVTransport;
    friend class OHInfo;
    friend class OHPlaylist;

    enum Options {
        upmpdNone,
        // If set, the MPD queue belongs to us, we shall clear
        // it as we like.
        upmpdOwnQueue, 
        // Export OpenHome services
        upmpdDoOH,
    };
    UpMpd(const string& deviceid, const string& friendlyname,
          const unordered_map<string, string>& xmlfiles,
          MPDCli *mpdcli, unsigned int opts = upmpdNone);
    ~UpMpd();

    const MpdStatus &getMpdStatus();

private:
    MPDCli *m_mpdcli;
    const MpdStatus *m_mpds;

    unsigned int m_options;
    vector<UpnpService*> m_services;
};


// "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3,"
// "http-get:*:audio/L16:DLNA.ORG_PN=LPCM,"
// "http-get:*:audio/x-flac:DLNA.ORG_PN=FLAC"
static const string 
upmpdProtocolInfo(
    "http-get:*:audio/wav:*,"
    "http-get:*:audio/wave:*,"
    "http-get:*:audio/x-wav:*,"
    "http-get:*:audio/x-aiff:*,"
    "http-get:*:audio/mpeg:*,"
    "http-get:*:audio/x-mpeg:*,"
    "http-get:*:audio/mp1:*,"
    "http-get:*:audio/aac:*,"
    "http-get:*:audio/flac:*,"
    "http-get:*:audio/x-flac:*,"
    "http-get:*:audio/m4a:*,"
    "http-get:*:audio/mp4:*,"
    "http-get:*:audio/x-m4a:*,"
    "http-get:*:audio/vorbis:*,"
    "http-get:*:audio/ogg:*,"
    "http-get:*:audio/x-ogg:*,"
    "http-get:*:audio/x-scpls:*,"
    "http-get:*:audio/L16;rate=11025;channels=1:*,"
    "http-get:*:audio/L16;rate=22050;channels=1:*,"
    "http-get:*:audio/L16;rate=44100;channels=1:*,"
    "http-get:*:audio/L16;rate=48000;channels=1:*,"
    "http-get:*:audio/L16;rate=88200;channels=1:*,"
    "http-get:*:audio/L16;rate=96000;channels=1:*,"
    "http-get:*:audio/L16;rate=176400;channels=1:*,"
    "http-get:*:audio/L16;rate=192000;channels=1:*,"
    "http-get:*:audio/L16;rate=11025;channels=2:*,"
    "http-get:*:audio/L16;rate=22050;channels=2:*,"
    "http-get:*:audio/L16;rate=44100;channels=2:*,"
    "http-get:*:audio/L16;rate=48000;channels=2:*,"
    "http-get:*:audio/L16;rate=88200;channels=2:*,"
    "http-get:*:audio/L16;rate=96000;channels=2:*,"
    "http-get:*:audio/L16;rate=176400;channels=2:*,"
    "http-get:*:audio/L16;rate=192000;channels=2:*"
    );

#endif /* _UPMPD_H_X_INCLUDED_ */
