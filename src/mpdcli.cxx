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
#include "mpdcli.hxx"

#include <mpd/client.h>

#include <stddef.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <string>
#include <memory>

#include "libupnpp/log.hxx"

#include "upmpd.hxx"
#include "conftree.hxx"
#include "execmd.h"

struct mpd_status;

using namespace std;
using namespace UPnPP;

#define M_CONN ((struct mpd_connection *)m_conn)

MPDCli::MPDCli(const string& host, int port, const string& pass)
    : m_conn(0), m_ok(false), m_premutevolume(0), m_cachedvolume(50),
      m_host(host), m_port(port), m_password(pass),
      m_externalvolumecontrol(false),
      m_lastinsertid(-1), m_lastinsertpos(-1), m_lastinsertqvers(-1)
{
    regcomp(&m_tpuexpr, "^[[:alpha:]]+://.+", REG_EXTENDED|REG_NOSUB);
    if (!openconn()) {
        return;
    }
    m_have_addtagid = checkForCommand("addtagid");

    m_ok = true;
    m_ok = updStatus();

    UPnPP::PTMutexLocker conflock(g_configlock);
    g_config->get("onstart", m_onstart);
    g_config->get("onplay", m_onplay);
    g_config->get("onstop", m_onstop);
    g_config->get("onvolumechange", m_onvolumechange);
    g_config->get("getexternalvolume", m_getexternalvolume);
    m_externalvolumecontrol = false;
    string value;
    if (g_config->get("externalvolumecontrol", value)) {
        m_externalvolumecontrol = atoi(value.c_str()) != 0;
    }
}

MPDCli::~MPDCli()
{
    if (m_conn) 
        mpd_connection_free(M_CONN);
    regfree(&m_tpuexpr);
}

// This is used on the auxiliary songcast mpd in a configuration where
// volume is normally controlled by an external script, but we still
// want to scale the Songcast stream.
void MPDCli::forceInternalVControl()
{
    m_getexternalvolume.clear();
    if (m_externalvolumecontrol)
        m_onvolumechange.clear();
    m_externalvolumecontrol = false;
}

bool MPDCli::looksLikeTransportURI(const string& path)
{
    return (regexec(&m_tpuexpr, path.c_str(), 0, 0, 0) == 0);
}

bool MPDCli::openconn()
{
    if (m_conn) {
        mpd_connection_free(M_CONN);
        m_conn = 0;
    }
    m_conn = mpd_connection_new(m_host.c_str(), m_port, 0);
    if (m_conn == NULL) {
        LOGERR("mpd_connection_new failed. No memory?" << endl);
        return false;
    }

    if (mpd_connection_get_error(M_CONN) != MPD_ERROR_SUCCESS) {
        showError("MPDCli::openconn");
        return false;
    }

    if(!m_password.empty()) {
        if (!mpd_run_password(M_CONN, m_password.c_str())) {
            LOGERR("Password wrong" << endl);
            return false;
        }
    }
    return true;
}

bool MPDCli::showError(const string& who)
{
    if (!ok()) {
        LOGERR("MPDCli::showError: bad state" << endl);
        return false;
    }

    int error = mpd_connection_get_error(M_CONN);
    if (error == MPD_ERROR_SUCCESS) {
        //LOGDEB("MPDCli::showError: " << who << " success !" << endl);
        return false;
    }
    LOGERR(who << " failed: " <<  mpd_connection_get_error_message(M_CONN) 
           << endl);
    if (error == MPD_ERROR_SERVER) {
        LOGERR(who << " server error: " << 
               mpd_connection_get_server_error(M_CONN) << endl);
    }

    if (error == MPD_ERROR_CLOSED)
        if (openconn())
            return true;
    return false;
}

#define RETRY_CMD(CMD) {                                \
    for (int i = 0; i < 2; i++) {                       \
        if ((CMD))                                      \
            break;                                      \
        if (i == 1 || !showError(#CMD))                 \
            return false;                               \
    }                                                   \
    }

#define RETRY_CMD_WITH_SLEEP(CMD) {                     \
    for (int i = 0; i < 2; i++) {                       \
        if ((CMD))                                      \
            break;                                      \
        sleep(1);                                       \
        if (i == 1 || !showError(#CMD))                 \
            return false;                               \
    }                                                   \
    }

bool MPDCli::updStatus()
{
    if (!ok()) {
        LOGERR("MPDCli::updStatus: bad state" << endl);
        return false;
    }

    mpd_status *mpds = 0;
    mpds = mpd_run_status(M_CONN);
    if (mpds == 0) {
        openconn();
        mpds = mpd_run_status(M_CONN);
        if (mpds == 0) {
            LOGERR("MPDCli::updStatus: can't get status" << endl);
            showError("MPDCli::updStatus");
        }
        return false;
    }

    if (m_externalvolumecontrol && !m_getexternalvolume.empty()) {
        string result;
        vector<string> cmd;
        cmd.push_back(m_getexternalvolume);
        if (ExecCmd::backtick(cmd, result)) {
            //LOGDEB("MPDCli::volume retrieved: " << result << endl);
            m_stat.volume = atoi(result.c_str());
        } else {
            LOGERR("MPDCli::updStatus: error retrieving volume: " <<
                   m_getexternalvolume << " failed\n");
        }
    } else {
	m_stat.volume = mpd_status_get_volume(mpds);
    }
    if (m_stat.volume >= 0) {
        m_cachedvolume = m_stat.volume;
    } else {
        m_stat.volume = m_cachedvolume;
    }

    m_stat.rept = mpd_status_get_repeat(mpds);
    m_stat.random = mpd_status_get_random(mpds);
    m_stat.single = mpd_status_get_single(mpds);
    m_stat.consume = mpd_status_get_consume(mpds);
    m_stat.qlen = mpd_status_get_queue_length(mpds);
    m_stat.qvers = mpd_status_get_queue_version(mpds);

    switch (mpd_status_get_state(mpds)) {
    case MPD_STATE_STOP:
        // Only execute onstop command if mpd was playing or paused
        if (!m_onstop.empty() && (m_stat.state == MpdStatus::MPDS_PLAY ||
                                  m_stat.state == MpdStatus::MPDS_PAUSE)) {
            if (system(m_onstop.c_str())) {
                LOGERR("MPDCli::updStatus: " << m_onstop << " failed "<< endl);
            }
        }
        m_stat.state = MpdStatus::MPDS_STOP;
        break;
    case MPD_STATE_PLAY:
        // Only execute onplay command if mpd was stopped
        if (!m_onplay.empty() && m_stat.state == MpdStatus::MPDS_STOP) {
            if (system(m_onplay.c_str())) {
                LOGERR("MPDCli::updStatus: " << m_onplay << " failed "<< endl);
            }
        }
        m_stat.state = MpdStatus::MPDS_PLAY;
        break;
    case MPD_STATE_PAUSE: m_stat.state = MpdStatus::MPDS_PAUSE;break;
    case MPD_STATE_UNKNOWN: 
    default:
        m_stat.state = MpdStatus::MPDS_UNK;
        break;
    }

    m_stat.crossfade = mpd_status_get_crossfade(mpds);
    m_stat.mixrampdb = mpd_status_get_mixrampdb(mpds);
    m_stat.mixrampdelay = mpd_status_get_mixrampdelay(mpds);
    m_stat.songpos = mpd_status_get_song_pos(mpds);
    m_stat.songid = mpd_status_get_song_id(mpds);
    if (m_stat.songpos >= 0) {
        string prevuri = m_stat.currentsong.uri;
        statSong(m_stat.currentsong);
        if (m_stat.currentsong.uri.compare(prevuri)) {
            m_stat.trackcounter++;
            m_stat.detailscounter = 0;
        }
        statSong(m_stat.nextsong, m_stat.songpos + 1);
    }

    m_stat.songelapsedms = mpd_status_get_elapsed_ms(mpds);
    m_stat.songlenms = mpd_status_get_total_time(mpds) * 1000;
    m_stat.kbrate = mpd_status_get_kbit_rate(mpds);
    const struct mpd_audio_format *maf = 
        mpd_status_get_audio_format(mpds);
    if (maf) {
        m_stat.bitdepth = maf->bits;
        m_stat.sample_rate = maf->sample_rate;
        m_stat.channels = maf->channels;
    } else {
        m_stat.bitdepth = m_stat.channels = m_stat.sample_rate = 0;
    }

    const char *err = mpd_status_get_error(mpds);
    if (err != 0)
        m_stat.errormessage.assign(err);

    mpd_status_free(mpds);
    return true;
}

bool MPDCli::checkForCommand(const string& cmdname)
{
    LOGDEB1("MPDCli::checkForCommand: " << cmdname << endl);
    bool found = false;

    RETRY_CMD(mpd_send_allowed_commands(M_CONN));
    struct mpd_pair *rep;
    do {
        rep = mpd_recv_command_pair(M_CONN);
        if (rep) {
            //LOGDEB("MPDCli::checkForCommand: name " <<  rep->name << 
            //       " value " << rep->value << endl);
            found = !cmdname.compare(rep->value);
            mpd_return_pair(M_CONN, rep);
            if (found)
                break;
        }
    } while (rep);

    if (!mpd_response_finish(M_CONN)) {
        LOGERR("MPDCli::checkForCommand: mpd_response_finish failed" << endl);
    }

    return found;
}

bool MPDCli::saveState(MpdState& st, int seekms)
{
    LOGDEB("MPDCli::saveState: seekms " << seekms << endl);
    if (!updStatus()) {
        LOGERR("MPDCli::saveState: can't retrieve current status\n");
        return false;
    }
    st.status = m_stat;
    if (seekms > 0) {
        st.status.songelapsedms = seekms;
    }
    st.queue.clear();
    if (!getQueueData(st.queue)) {
        LOGERR("MPDCli::saveState: can't retrieve current playlist\n");
        return false;
    }
    return true;
}

bool MPDCli::restoreState(const MpdState& st)
{
    LOGDEB("MPDCli::restoreState: seekms " << st.status.songelapsedms << endl);
    clearQueue();
    for (unsigned int i = 0; i < st.queue.size(); i++) {
        if (insert(st.queue[i].uri, i, st.queue[i]) < 0) {
            LOGERR("MPDCli::restoreState: insert failed\n");
            return false;
        }
    }
    repeat(st.status.rept);
    random(st.status.random);
    single(st.status.single);
    consume(st.status.consume);
    m_cachedvolume = st.status.volume;
    //no need to set volume if it is controlled external
    if (!m_externalvolumecontrol)
        mpd_run_set_volume(M_CONN, st.status.volume);

    if (st.status.state == MpdStatus::MPDS_PAUSE ||
        st.status.state == MpdStatus::MPDS_PLAY) {
        // I think that the play is necessary and we can't just do
        // pause/seek from stop state. To be verified.
        play(st.status.songpos);
        if (st.status.songelapsedms > 0)
            seek(st.status.songelapsedms/1000);
        if (st.status.state == MpdStatus::MPDS_PAUSE)
            pause(true);
        if (!m_externalvolumecontrol)
            mpd_run_set_volume(M_CONN, st.status.volume);
    }
    return true;
}


bool MPDCli::statSong(UpSong& upsong, int pos, bool isid)
{
    //LOGDEB1("MPDCli::statSong. isid " << isid << " id/pos " << pos << endl);
    if (!ok())
        return false;

    struct mpd_song *song;
    if (isid == false) {
        if (pos == -1) {
            RETRY_CMD(song = mpd_run_current_song(M_CONN));
        } else {
            RETRY_CMD(song = mpd_run_get_queue_song_pos(M_CONN, 
                                                        (unsigned int)pos));
        }
    } else {
        RETRY_CMD(song = mpd_run_get_queue_song_id(M_CONN, (unsigned int)pos));
    }
    if (song == 0) {
        LOGERR("mpd_run_current_song failed" << endl);
        return false;
    }
    mapSong(upsong, song);
    mpd_song_free(song);
    return true;
}    

UpSong&  MPDCli::mapSong(UpSong& upsong, struct mpd_song *song)
{
    //LOGDEB1("MPDCli::mapSong" << endl);
    const char *cp;

    cp = mpd_song_get_uri(song);
    if (cp != 0)
        upsong.uri = cp;
    else 
        upsong.uri.clear();
    // If the URI looks like a local file
    // name, replace with a bogus http uri. This is to fool
    // Bubble UPnP into accepting to play them (it does not
    // actually need an URI as it's going to use seekid, but
    // it believes it does).
    if (!looksLikeTransportURI(upsong.uri)) {
        //LOGDEB("MPDCli::mapSong: id " << upsong.mpdid << 
        // " replacing [" << upsong.uri << "]" << endl);
        upsong.uri = "http://127.0.0.1/" + upsong.uri;
    }
    cp = mpd_song_get_tag(song, MPD_TAG_NAME, 0);
    if (cp != 0)
        upsong.name = cp;
    else
        upsong.name.clear();
    cp = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
    if (cp != 0)
        upsong.artist = cp;
    else
        upsong.artist.clear();
    if (upsong.artist.empty())
        upsong.artist = upsong.name;
    cp = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
    if (cp != 0)
        upsong.album = cp;
    else
        upsong.album.clear();
    cp = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
    if (cp != 0) 
        upsong.title = cp;
    else
        upsong.title.clear();
    cp = mpd_song_get_tag(song, MPD_TAG_TRACK, 0);
    if (cp != 0)
        upsong.tracknum = cp;
    else 
        upsong.tracknum.clear();
    cp = mpd_song_get_tag(song, MPD_TAG_GENRE, 0);
    if (cp != 0)
        upsong.genre = cp;
    else
        upsong.genre.clear();

    upsong.duration_secs = mpd_song_get_duration(song);
    upsong.mpdid = mpd_song_get_id(song);

//    LOGDEB1("MPDCli::mapSong: got mpdid " << upsong.mpdid << " " << 
//            upsong.dump() << endl);
    return upsong;
}

bool MPDCli::setVolume(int volume, bool isMute)
{
    LOGDEB("MPDCli::setVolume. extvc " << m_externalvolumecontrol << endl);
    if (!ok()) {
        return false;
    }

    // ??MPD does not want to set the volume if not active.??
    // This does not seem to be the case with recent MPD versions
    if (!(m_stat.state == MpdStatus::MPDS_PLAY) &&
        !(m_stat.state == MpdStatus::MPDS_PAUSE)) {
        LOGDEB1("MPDCli::setVolume: not active" << endl);
    }

    LOGDEB("MPDCli::setVolume: vol " << volume << " isMute " << isMute << endl);

    if (isMute) {
        if (volume) {
            // If we're already not muted, do nothing
            if (m_premutevolume == 0)
                return true;
            // Restore premute volume
            LOGDEB("MPDCli::setVolume: restoring premute " << m_premutevolume 
                   << endl);
            volume = m_stat.volume = m_premutevolume;
            m_premutevolume = 0;
        } else {
            // If we're already muted, do nothing
            if (m_premutevolume > 0)
                return true;
            if (m_cachedvolume > 0) {
                m_premutevolume = m_cachedvolume;
            }
        }
    }
        
    if (volume < 0)
        volume = 0;
    else if (volume > 100)
        volume = 100;
    
    if (!(m_externalvolumecontrol)) {
    	RETRY_CMD(mpd_run_set_volume(M_CONN, volume));
    }
    if (!m_onvolumechange.empty()) {
        ExecCmd ecmd;
        vector<string> args;
        stringstream ss;
        ss << volume;
        args.push_back(ss.str());
        if (ecmd.doexec(m_onvolumechange, args)) {
            // doexec returns the exit status, should be zero
            LOGDEB("MPDCli::setVolume: " << m_onvolumechange <<
                   " failed " << volume << endl);
        }
    }
    m_stat.volume = volume;
    m_cachedvolume = volume;
    return true;
}

int MPDCli::getVolume()
{
    //LOGDEB1("MPDCli::getVolume" << endl);
    return m_stat.volume >= 0 ? m_stat.volume : m_cachedvolume;
}

bool MPDCli::togglePause()
{
    LOGDEB("MPDCli::togglePause" << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_toggle_pause(M_CONN));
    return true;
}

bool MPDCli::pause(bool onoff)
{
    LOGDEB("MPDCli::pause" << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_pause(M_CONN, onoff));
    return true;
}

bool MPDCli::play(int pos)
{
    LOGDEB("MPDCli::play(pos=" << pos << ")" << endl);
    if (!ok())
        return false;
    if (!m_onstart.empty()) {
        if (system(m_onstart.c_str())) {
            LOGERR("MPDCli::play: " << m_onstart << " failed "<< endl);
        }
    }
    if (pos >= 0) {
        RETRY_CMD(mpd_run_play_pos(M_CONN, (unsigned int)pos));
    } else {
        RETRY_CMD(mpd_run_play(M_CONN));
    }
    return updStatus();
}

bool MPDCli::playId(int id)
{
    LOGDEB("MPDCli::playId(id=" << id << ")" << endl);
    if (!ok())
        return false;
    if (!m_onstart.empty()) {
        if (system(m_onstart.c_str())) {
            LOGERR("MPDCli::playId: " << m_onstart << " failed "<< endl);
        }
    }
    RETRY_CMD(mpd_run_play_id(M_CONN, (unsigned int)id));
    return updStatus();
}
bool MPDCli::stop()
{
    LOGDEB("MPDCli::stop" << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_stop(M_CONN));
    return true;
}
bool MPDCli::seek(int seconds)
{
    if (!updStatus())
        return -1;
    LOGDEB("MPDCli::seek: pos:"<<m_stat.songpos<<" seconds: "<< seconds<<endl);
    RETRY_CMD(mpd_run_seek_pos(M_CONN, m_stat.songpos, (unsigned int)seconds));
    return true;
}

bool MPDCli::next()
{
    LOGDEB("MPDCli::next" << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_next(M_CONN));
    return true;
}
bool MPDCli::previous()
{
    LOGDEB("MPDCli::previous" << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_previous(M_CONN));
    return true;
}
bool MPDCli::repeat(bool on)
{
    LOGDEB("MPDCli::repeat:" << on << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_repeat(M_CONN, on));
    return true;
}

bool MPDCli::consume(bool on)
{
    LOGDEB("MPDCli::consume:" << on << endl);
    if (!ok())
        return false;

    RETRY_CMD(mpd_run_consume(M_CONN, on));
    return true;
}
bool MPDCli::random(bool on)
{
    LOGDEB("MPDCli::random:" << on << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_random(M_CONN, on));
    return true;
}
bool MPDCli::single(bool on)
{
    LOGDEB("MPDCli::single:" << on << endl);
    if (!ok())
        return false;
    RETRY_CMD(mpd_run_single(M_CONN, on));
    return true;
}

bool MPDCli::send_tag(const char *cid, int tag, const string& data)
{
    if (!mpd_send_command(M_CONN, "addtagid", cid, 
                          mpd_tag_name(mpd_tag_type(tag)),
                          data.c_str(), NULL)) {
        LOGERR("MPDCli::send_tag: mpd_send_command failed" << endl);
        return false;
    }

    if (!mpd_response_finish(M_CONN)) {
        LOGERR("MPDCli::send_tag: mpd_response_finish failed\n");
        showError("MPDCli::send_tag");
        return false;
    }
    return true;
}

static const string upmpdcli_comment("client=upmpdcli;");

bool MPDCli::send_tag_data(int id, const UpSong& meta)
{
    LOGDEB1("MPDCli::send_tag_data" << endl);
    if (!m_have_addtagid)
        return false;

    char cid[30];
    sprintf(cid, "%d", id);

    if (!send_tag(cid, MPD_TAG_ARTIST, meta.artist))
        return false;
    if (!send_tag(cid, MPD_TAG_ALBUM, meta.album))
        return false;
    if (!send_tag(cid, MPD_TAG_TITLE, meta.title))
        return false;
    if (!send_tag(cid, MPD_TAG_TRACK, meta.tracknum))
        return false;
    if (!send_tag(cid, MPD_TAG_COMMENT, upmpdcli_comment))
        return false;
    return true;
}

int MPDCli::insert(const string& uri, int pos, const UpSong& meta)
{
    LOGDEB("MPDCli::insert at :" << pos << " uri " << uri << endl);
    if (!ok())
        return -1;

    RETRY_CMD((m_lastinsertid = 
               mpd_run_add_id_to(M_CONN, uri.c_str(), (unsigned)pos)) != -1);
    
    if (m_have_addtagid)
        send_tag_data(m_lastinsertid, meta);

    m_lastinsertpos = pos;
    updStatus();
    m_lastinsertqvers = m_stat.qvers;
    return m_lastinsertid;
}

int MPDCli::insertAfterId(const string& uri, int id, const UpSong& meta)
{
    LOGDEB("MPDCli::insertAfterId: id " << id << " uri " << uri << endl);
    if (!ok())
        return -1;

    // id == 0 means insert at start
    if (id == 0) {
        return insert(uri, 0, meta);
    }
    updStatus();

    int newpos = 0;
    if (m_lastinsertid == id && m_lastinsertpos >= 0 &&
        m_lastinsertqvers == m_stat.qvers) {
        newpos = m_lastinsertpos + 1;
    } else {
        // Translate input id to insert position
        vector<mpd_song*> songs;
        if (!getQueueSongs(songs)) {
            return false;
        }
        for (unsigned int pos = 0; pos < songs.size(); pos++) {
            unsigned int qid = mpd_song_get_id(songs[pos]);
            if (qid == (unsigned int)id || pos == songs.size() -1) {
                newpos = pos + 1;
                break;
            }
        }
        freeSongs(songs);
    }
    return insert(uri, newpos, meta);
}

bool MPDCli::clearQueue()
{
    LOGDEB("MPDCli::clearQueue " << endl);
    if (!ok())
        return -1;

    RETRY_CMD(mpd_run_clear(M_CONN));
    return true;
}

bool MPDCli::deleteId(int id)
{
    LOGDEB("MPDCli::deleteId " << id << endl);
    if (!ok())
        return -1;
    // It seems that mpd will sometimes get in a funny state, esp.
    // after failed statsongs. The exact mechanism is a mystery, but
    // retrying the failed deletes with a bit of wait seems to help a
    // lot, and this happens seldom enough that this is not a
    // significant performance issue
    RETRY_CMD_WITH_SLEEP(mpd_run_delete_id(M_CONN, (unsigned)id));
    return true;
}

bool MPDCli::deletePosRange(unsigned int start, unsigned int end)
{
    LOGDEB("MPDCli::deletePosRange [" << start << ", " << end << "[" << endl);
    if (!ok())
        return -1;

    RETRY_CMD(mpd_run_delete_range(M_CONN, start, end));
    return true;
}


bool MPDCli::statId(int id)
{
    LOGDEB("MPDCli::statId " << id << endl);
    if (!ok())
        return -1;

    mpd_song *song = mpd_run_get_queue_song_id(M_CONN, (unsigned)id);
    if (song) {
        mpd_song_free(song);
        return true;
    }
    return false;
}

bool MPDCli::getQueueSongs(vector<mpd_song*>& songs)
{
    //LOGDEB1("MPDCli::getQueueSongs" << endl);
    songs.clear();

    RETRY_CMD(mpd_send_list_queue_meta(M_CONN));

    struct mpd_song *song;
    while ((song = mpd_recv_song(M_CONN)) != NULL) {
        songs.push_back(song);
    }
    
    if (!mpd_response_finish(M_CONN)) {
        LOGERR("MPDCli::getQueueSongs: mpd_list_queue_meta failed"<< endl);
        return false;
    }
    LOGDEB("MPDCli::getQueueSongs: " << songs.size() << " songs " << endl);
    return true;
}

void MPDCli::freeSongs(vector<mpd_song*>& songs)
{
    LOGDEB1("MPDCli::freeSongs" << endl);
    for (vector<mpd_song*>::iterator it = songs.begin();
         it != songs.end(); it++) {
        mpd_song_free(*it);
    }
}

bool MPDCli::getQueueData(std::vector<UpSong>& vdata)
{
    LOGDEB("MPDCli::getQueueData" << endl);
    vector<mpd_song*> songs;
    if (!getQueueSongs(songs)) {
        return false;
    }
    vdata.reserve(songs.size());
    UpSong usong;
    for (unsigned int pos = 0; pos < songs.size(); pos++) {
        vdata.push_back(mapSong(usong, songs[pos]));
    }
    freeSongs(songs);
    return true;
}

int MPDCli::curpos()
{
    if (!updStatus())
        return -1;
    LOGDEB("MPDCli::curpos: pos: " << m_stat.songpos << " id " 
           << m_stat.songid << endl);
    return m_stat.songpos;
}



#ifdef MPDCLI_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <iostream>
using namespace std;

#include "mpdcli.hxx"

static char *thisprog;

static char usage [] =
"  \n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s	  0x2 
#define OPT_b	  0x4 

int main(int argc, char **argv)
{
  int count = 10;
    
  thisprog = argv[0];
  argc--; argv++;

  while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
      /* Cas du "adb - core" */
      Usage();
    while (**argv)
      switch (*(*argv)++) {
      case 's':	op_flags |= OPT_s; break;
      case 'b':	op_flags |= OPT_b; if (argc < 2)  Usage();
	if ((sscanf(*(++argv), "%d", &count)) != 1) 
	  Usage(); 
	argc--; 
	goto b1;
      default: Usage();	break;
      }
  b1: argc--; argv++;
  }

  if (argc != 0)
    Usage();

  MPDCli cli("localhost");
  if (!cli.ok()) {
      cerr << "Cli connection failed" << endl;
      return 1;
  }
  const MpdStatus& status = cli.getStatus();
  
  if (status.state != MpdStatus::MPDS_PLAY) {
      cerr << "Not playing" << endl;
      return 1;
  }

  unsigned int seektarget = (status.songlenms - 4500)/1000;
  cerr << "songpos " << status.songpos << " songid " << status.songid <<
      " seeking to " << seektarget << " seconds" << endl;

  if (!cli.seek(seektarget)) {
      cerr << "Seek failed" << endl;
      return 1;
  }
  return 0;
}

#endif
