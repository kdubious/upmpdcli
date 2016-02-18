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
#ifndef _MPDCLI_H_X_INCLUDED_
#define _MPDCLI_H_X_INCLUDED_

#include <regex.h>                      // for regex_t
#include <string>                       // for string
#include <cstdio>
#include <vector>                       // for vector
#include <memory>

struct mpd_song;

class UpSong {
public:
    UpSong() : duration_secs(0), mpdid(0) {}
    void clear() {
        uri.clear(); 
        artist.clear(); 
        album.clear();
        title.clear();
        tracknum.clear();
        genre.clear();
        duration_secs = mpdid = 0;
    }
    std::string uri;
    std::string name; // only set for radios apparently
    std::string artist;
    std::string album;
    std::string title;
    std::string tracknum;
    std::string genre;
    std::string artUri; // This does not come from mpd, but we sometimes add it
    unsigned int duration_secs;
    int mpdid;
    std::string dump() {
        return std::string("Uri [") + uri + "] Artist [" + artist + "] Album ["
            +  album + "] Title [" + title + "] Tno [" + tracknum + "]";
    }
};

class MpdStatus {
public:
    MpdStatus() : trackcounter(0), detailscounter(0) {}

    enum State {MPDS_UNK, MPDS_STOP, MPDS_PLAY, MPDS_PAUSE};

    int volume;
    bool rept;
    bool random;
    bool single;
    bool consume;
    int qlen;
    int qvers;
    State state;
    unsigned int crossfade;
    float mixrampdb;
    float mixrampdelay;
    int songpos;
    int songid;
    unsigned int songelapsedms; //current ms
    unsigned int songlenms; // song millis
    unsigned int kbrate;
    unsigned int sample_rate;
    unsigned int bitdepth;
    unsigned int channels;
    std::string errormessage;
    UpSong currentsong;
    UpSong nextsong;

    // Synthetized fields
    int trackcounter;
    int detailscounter;
    bool externalvolumecontrol;
    std::string onvolumechange;
    std::string getexternalvolume;
};

// Complete Mpd State
struct MpdState {
    MpdStatus status;
    std::vector<UpSong> queue;
};

class MPDCli {
public:
    MPDCli(const std::string& host, int port = 6600, 
           const std::string& pass="", const std::string& onstart="",
           const std::string& onplay="", const std::string& onstop="",
           const std::string& onvolumechange="", 
	   const std::string& getexternalvolume="",
	   bool externalvolumecontrol = false);
    ~MPDCli();
    bool ok() {return m_ok && m_conn;}
    bool setVolume(int ivol, bool isMute = false);
    int  getVolume();
    bool togglePause();
    bool pause(bool onoff);
    bool play(int pos = -1);
    bool playId(int id = -1);
    bool stop();
    bool next();
    bool previous();
    bool repeat(bool on);
    bool random(bool on);
    bool single(bool on);
    bool consume(bool on);
    bool seek(int seconds);
    bool clearQueue();
    int insert(const std::string& uri, int pos, const UpSong& meta);
    // Insert after given id. Returns new id or -1
    int insertAfterId(const std::string& uri, int id, const UpSong& meta);
    bool deleteId(int id);
    // start included, end excluded
    bool deletePosRange(unsigned int start, unsigned int end);
    bool statId(int id);
    int curpos();
    bool getQueueData(std::vector<UpSong>& vdata);
    bool statSong(UpSong& usong, int pos = -1, bool isId = false);
    UpSong& mapSong(UpSong& usong, struct mpd_song *song);
    
    const MpdStatus& getStatus()
    {
        updStatus();
        return m_stat;
    }

    // Copy complete mpd state. If seekms is > 0, this is the value to
    // save (sometimes useful if mpd was stopped)
    bool saveState(MpdState& st, int seekms);
    bool restoreState(const MpdState& st);
    
private:
    void *m_conn;
    bool m_ok;
    MpdStatus m_stat;
    // Saved volume while muted.
    int m_premutevolume;
    // Volume that we use when MPD is stopped (does not return a
    // volume in the status)
    int m_cachedvolume; 
    std::string m_host;
    int m_port;
    std::string m_password;
    std::string m_onstart;
    std::string m_onplay;
    std::string m_onstop;
    regex_t m_tpuexpr;
    // addtagid command only exists for mpd 0.19 and later.
    bool m_have_addtagid; 
    // Position and id of last insertion: if the new request is to
    // insert after this id, and the queue did not change, we compute
    // the new position from the last one instead of re-reading the
    // queue for looking up the id position. This saves a huge amount
    // of time.
    int m_lastinsertid;
    int m_lastinsertpos;
    int m_lastinsertqvers;

    bool openconn();
    bool updStatus();
    bool getQueueSongs(std::vector<mpd_song*>& songs);
    void freeSongs(std::vector<mpd_song*>& songs);
    bool showError(const std::string& who);
    bool looksLikeTransportURI(const std::string& path);
    bool checkForCommand(const std::string& cmdname);
    bool send_tag(const char *cid, int tag, const std::string& data);
    bool send_tag_data(int id, const UpSong& meta);
};


#endif /* _MPDCLI_H_X_INCLUDED_ */
