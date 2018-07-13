/* Copyright (C) 2017-2018 J.F.Dockes
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
#include "spotiproxy.h"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <mutex>

#include <libspotify/api.h>
#include <FLAC/stream_encoder.h>

#include "log.h"
#include "smallut.h"

using namespace std;
using namespace std::placeholders;

/* mopidy appkey from mopidy_spotify/spotify_appkey.key */
/* No idea how to get a new one now that the lib is deprecated, sorry */
const vector<uint8_t> g_appkey {
0x01,0xCF,0x89,0x0F,0xDE,0x9F,0xD6,0x21,0x50,0x16,0x8E,0xD4,0x33,0x7F,0x73,0x82,
0xC1,0x52,0xC7,0x4E,0x85,0x47,0x20,0x8D,0x53,0xB9,0x22,0x5E,0x3D,0xC5,0x2B,0x09,
0xE9,0xCF,0x64,0x2F,0x64,0x85,0xCF,0xC3,0x4B,0x7E,0xEB,0x38,0x06,0x28,0x25,0x6E,
0xD1,0xD5,0xFE,0x47,0xF7,0x7E,0x4C,0x90,0x0E,0x9F,0xB8,0x0B,0x98,0x1A,0x14,0x2E,
0x24,0xBF,0xDD,0x71,0x73,0x6D,0xC5,0xBD,0xF3,0xB2,0x81,0x9E,0x10,0x79,0x7C,0x33,
0x13,0xAC,0x30,0x03,0x97,0x3E,0x74,0x87,0xB6,0x95,0x7C,0xC1,0xEA,0x64,0x89,0xE2,
0x0D,0xDE,0xA2,0xDA,0xB7,0xBC,0xF9,0x2B,0xBB,0xDF,0xB2,0x97,0x34,0xCE,0xBB,0x79,
0xEC,0x2F,0xA2,0xEE,0xF1,0x21,0xF7,0xCC,0xF3,0xC9,0x75,0x90,0x15,0x3F,0xBB,0xAA,
0xC2,0xC9,0x64,0x39,0x07,0xD8,0x57,0x0F,0x09,0x28,0x71,0x47,0x04,0x48,0xF0,0x54,
0x8E,0x4D,0xD3,0x2B,0xC3,0xA3,0xF8,0x2B,0x22,0xC1,0xC2,0x86,0xB3,0x67,0xB9,0xBE,
0x16,0x70,0xE2,0xAB,0x17,0x76,0xE9,0xAD,0x08,0x50,0xCF,0xD8,0x0B,0x32,0xC6,0x34,
0x64,0x4B,0x6F,0xC4,0x20,0x62,0xBD,0x48,0xD1,0xFB,0x57,0x5D,0x29,0xBC,0x10,0x89,
0xC3,0xB5,0x9F,0x57,0xFB,0x74,0x4E,0x01,0x59,0xEB,0xAC,0x99,0xB7,0x95,0x70,0x2C,
0x12,0xE8,0x60,0xE0,0x5F,0x3E,0x56,0xEB,0x74,0x28,0xC0,0x5D,0x2C,0x45,0x09,0x0F,
0x1F,0x96,0x6F,0x99,0x60,0x25,0x08,0x89,0xD0,0xB3,0xFA,0xAD,0x86,0x17,0xE7,0x30,
0xA9,0x5B,0xE7,0x61,0xAC,0x3A,0xFB,0xCD,0xC6,0xFB,0x8A,0xD0,0x19,0xC8,0xBE,0xD8,
0xD5,0xA7,0xBB,0x04,0xE5,0x1D,0xA4,0x00,0x45,0xBD,0x84,0x7B,0xE2,0x7B,0x26,0x5D,
0x6E,0x4C,0x42,0xEF,0xC2,0x72,0x49,0x69,0x9F,0x7D,0x66,0x9E,0x95,0xAA,0x94,0xCF,
0x89,0xC8,0x4C,0xFD,0xD5,0x41,0xE7,0x64,0xA1,0xE8,0xEE,0xA7,0x98,0xD6,0xCF,0x1A,
0x9B,0x03,0x9D,0x93,0xB7,0x5F,0x3C,0xA4,0x36,0xE1,0xF3,0x07,0x4D,0xEA,0x01,0x1D,
0x3D};

static SpotiProxy *theSpotiProxy;
static SpotiProxy::Internal *theSPP;

static int g_notify_do;
static sp_session_callbacks session_callbacks;
static sp_session_config spconfig;

// Forward decls
static void login_cb(sp_session *sess, sp_error error);
static void log_message(sp_session *session, const char *msg);
static void notify_main_thread(sp_session *sess);
static void metadata_updated(sp_session *sess);
static int music_delivery(sp_session *sess, const sp_audioformat *format,
                          const void *frames, int num_frames);
static void end_of_track(sp_session *sess);
static void play_token_lost(sp_session *sess);

class SpotiProxy::Internal {
public:

    /* The constructor logs us in */
    Internal(const string& u, const string& p,
             const string& cd, const string& sd)
        : user(u), pass(p), cachedir(cd), confdir(sd) {
        theSPP = this;
        session_callbacks.logged_in = login_cb;
        session_callbacks.log_message = log_message;
        session_callbacks.notify_main_thread = notify_main_thread;
        session_callbacks.metadata_updated = metadata_updated;
	session_callbacks.music_delivery = music_delivery;
        session_callbacks.play_token_lost = play_token_lost;
	session_callbacks.end_of_track = end_of_track,

        spconfig.api_version = SPOTIFY_API_VERSION;
        spconfig.application_key = &g_appkey[0];
        spconfig.application_key_size = g_appkey.size();
        spconfig.user_agent = "upmpdcli-spotiproxy";
        spconfig.callbacks = &session_callbacks;
        spconfig.cache_location = cachedir.c_str();
        spconfig.settings_location = confdir.c_str();

        sperror = sp_session_create(&spconfig, &sp);
        if (SP_ERROR_OK != sperror) {
            registerError(sperror);
            return;
        }
        sp_session_login(sp, user.c_str(), pass.c_str(), 1, NULL);
        wait_for("Login", [] (SpotiProxy::Internal *o) {return o->logged_in;});
        if (logged_in) {
            LOGDEB("Spotify: " << user << " logged in ok\n");
        } else {
            LOGERR("Spotify: " << user << " log in failed\n");
        }
    }

    // Wait for a state change, tested by a function parameter.
    bool wait_for(const string& who,
                  std::function<bool(SpotiProxy::Internal *)> testit) {
        int next_timeout = 0;
        for (;;) {
            if (!g_notify_do) {
                unique_lock<mutex> lock(spmutex);
                if (testit(this) || sperror != SP_ERROR_OK) {
                    return sperror == SP_ERROR_OK;
                }
                if (next_timeout == 0) {
                    LOGDEB1(who << " Waiting\n");
                    spcv.wait(lock);
                } else {
                    LOGDEB1(who << " waiting " << next_timeout << " mS\n");
                    spcv.wait_for(
                        lock, std::chrono::milliseconds(next_timeout));
                }
            }
            do {
                g_notify_do = 0;
                LOGDEB1(who << " Calling process_events\n");
                sp_session_process_events(sp, &next_timeout);
                LOGDEB1(who << " After process_event, next_timeout " <<
                        next_timeout << " notify_do " << g_notify_do << endl);
            } while (next_timeout == 0);
        }
    }        

    void unloadTrack() {
        LOGDEB1("unloadTrack: getting lock\n");
        unique_lock<mutex> lock(spmutex);
	if (sp && curtrack) {
            sp_track_release(curtrack);
            sp_session_player_unload(sp);
	}
        reason.clear();
        sperror = SP_ERROR_OK;
        curtrack = nullptr;
        track_playing = false;
        track_duration = 0;
    }
    
    ~Internal() {
        unloadTrack();
        if (sp && logged_in) {
            LOGDEB("Logging out\n");
            sp_session_logout(sp);
        }
    }

    void registerError(sp_error error) {
        reason += string(sp_error_message(error)) + " ";
        sperror = error;
    }

    string user;
    string pass;
    string cachedir;
    string confdir;
    sp_session *sp{nullptr};
    bool logged_in{false};
    condition_variable spcv;
    mutex spmutex;
    string reason;
    sp_error sperror{SP_ERROR_OK};
    sp_track *curtrack{nullptr};
    bool track_playing{false};
    int track_duration{0};
    AudioSink sink{nullptr};
};

class Cleaner {
public:
    ~Cleaner() {
        delete theSpotiProxy;
    }
};
static Cleaner cleaner;

static void login_cb(sp_session *sess, sp_error error)
{
    const char *me = "login_cb";
    LOGDEB1(me << " error " << error << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        // ??
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);

    if (SP_ERROR_OK == error) {
        theSPP->logged_in = true;
    } else {
        theSPP->registerError(error);
    }
    theSPP->spcv.notify_all();
}

static void log_message(sp_session *session, const char *msg)
{
    LOGDEB(msg);
}

static void metadata_updated(sp_session *sess)
{
    const char *me = "metadata_updated";
    LOGDEB1(me << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);
    theSPP->spcv.notify_all();
}

#include <fcntl.h>

static int music_delivery(sp_session *sess, const sp_audioformat *format,
                          const void *frames, int num_frames)
{
    const char *me = "music_delivery";
    static int counter;
    if ((counter++ %100) == 0) {
        LOGDEB1("music_delivery: " << num_frames << " frames " <<
                " samplerate " << format->sample_rate << " channels " <<
                format->channels << endl);
    }

    if (num_frames == 0) {
        LOGDEB("music_delivery: called with 0 frames\n");
        return 0;
    }
    if (nullptr == theSPP) {
        return -1;
    }
    if (num_frames > 4096) {
        // ?? It seems that notify_main_thread() is not called when it
        // should at the end of track, so process_events() is not
        // either, nor end_of_track() (called from the main thread, so
        // from process_events()). So wake-up the main thread ourself.
        // This is weird of course, I don't understand why it's
        // needed.
        LOGDEB(me << ": got silence buffer\n");
        // Silence buffer: end of real track
#if 0
        {
            unique_lock<mutex> lock(theSPP->spmutex);
            theSPP->track_playing = false;
            theSPP->track_duration = 0;
            theSPP->spcv.notify_all();
        }
#else
        // It seems a bit reckless to call unloadtrack from here,
        // because it calls back into libspotify to release and unload
        // the track. But does not seem to cause a problem. The
        // alternative would be to start an auxiliary thread just for
        // monitoring the play state and calling player_unload() (else
        // we continue receiving silence buffers). Can't count on the
        // user thread to do it (e.g when called through SpotiFetch,
        // nobody calls waitForEndOfPlay()).
        theSPP->unloadTrack();
#endif
        theSPP->sink(frames, 0, format->channels, format->sample_rate);
        return num_frames;
    }

    return theSPP->sink(frames, num_frames, format->channels,
                        format->sample_rate);
}

#if 0
// Spotify probably does this to adjust its send rate, but it's not
// clear how to implement it in general, nor does it seems to be
// needed. Would need another function callback to work (in addition
// to sink())
static void get_audio_buffer_stats(sp_session *sess,
                                   sp_audio_buffer_stats *stats)
{
    const char *me = "get_audio_buffer_stats";
    LOGDEB(me << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        // ??
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);

    stats->samples = 0;
    stats->stutter = 0;
}
#endif

static void end_of_track(sp_session *sess)
{
    const char *me = "end_of_track";
    LOGDEB1(me << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        // ??
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);

    theSPP->track_playing = false;
    theSPP->track_duration = 0;
    theSPP->spcv.notify_all();
}

static void play_token_lost(sp_session *sess)
{
    const char *me = "play_token_lost";
    LOGDEB(me << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        // ??
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);

    if (nullptr != theSPP->curtrack) {
        sp_session_player_unload(theSPP->sp);
        theSPP->curtrack = nullptr;
    }
    theSPP->spcv.notify_all();
}

static void notify_main_thread(sp_session *sess)
{
    const char *me = "notify_main_thread";
    LOGDEB1(me << "\n");
    if (nullptr == theSPP) {
        LOGERR(me << " no SPP ??\n");
        // ??
        return;
    }
    unique_lock<mutex> lock(theSPP->spmutex);

    g_notify_do = 1;
    theSPP->spcv.notify_all();
}

static string o_user, o_password, o_cachedir, o_settingsdir;
void SpotiProxy::setParams(
        const std::string& user, const std::string& pass,
        const std::string& cachedir, const std::string& settingsdir)
{
    o_user = user;
    o_password = pass;
    o_cachedir = cachedir;
    o_settingsdir = settingsdir;
}

SpotiProxy *SpotiProxy::getSpotiProxy(
    const string& u, const string& p, const string& cached, const string& confd)
{
    LOGDEB1("getSpotiProxy\n");
    if (theSpotiProxy) {
        LOGDEB1("getSpotiProxy: already created\n");
        if ((u.empty() && p.empty()) ||
            (theSpotiProxy->m->user == u && theSpotiProxy->m->pass == p)) {
            return theSpotiProxy;
        } else {
            return nullptr;
        }
    } else {
        string user(u.empty() ? o_user : u);
        string pass(p.empty() ? o_password : p);
        LOGDEB("getSpotiProxy: creating for user " << user <<"\n");
        theSpotiProxy = new SpotiProxy(user, pass,  
                                       cached.empty() ? o_cachedir : cached,
                                       confd.empty() ? o_settingsdir : confd);
        return theSpotiProxy;
    }
}


SpotiProxy::SpotiProxy(const string& user, const string& password,
                       const string& cd, const string& sd)
    : m(std::unique_ptr<Internal>(new Internal(user, password, cd, sd)))
{
}

SpotiProxy::~SpotiProxy() {}

bool SpotiProxy::playTrack(const string& trackid, AudioSink sink,
                             int seekmsecs)
{
    if (!startPlay(trackid, sink, seekmsecs)) {
        return false;
    }
    return waitForEndOfPlay();
}

bool SpotiProxy::startPlay(const string& trackid, AudioSink sink,
                             int seekmsecs)
{
    LOGDEB("SpotiProxy::startPlay: id " << trackid << " at " <<
           seekmsecs / 1000 << " S\n");
    string trackref("spotify:track:");
    trackref += trackid;
    sp_link *link = sp_link_create_from_string(trackref.c_str());
    if (!link) {
        LOGERR("SpotiProxy:startPlay: link creation failed\n");
        return false;
    }
    m->curtrack = sp_link_as_track(link);
    sp_track_add_ref(m->curtrack);
    sp_link_release(link);

    m->sink = sink;
    
    if (!m->wait_for("startPlay", [](SpotiProxy::Internal *o) {
                return sp_track_error(o->curtrack) == SP_ERROR_OK;})) {
        LOGERR("playTrackId: error waiting for track metadata ready\n");
        return false;
    }

    theSPP->track_duration = sp_track_duration(m->curtrack);
    LOGDEB("SpotiProxy::startPlay: NOW PLAYING "<< sp_track_name(m->curtrack) <<
           " duration " << theSPP->track_duration << endl);
    sp_session_player_load(m->sp, m->curtrack);
    if (seekmsecs) {
        sp_session_player_seek(m->sp, seekmsecs);
    }
    sp_session_player_play(m->sp, 1);
    m->track_playing = true;
    return true;
}

bool SpotiProxy::waitForEndOfPlay()
{
    if (!m->wait_for("waitForEndOfPlay", [](SpotiProxy::Internal *o) {
                return o->track_playing == false;})) {
        LOGERR("playTrackId: error waiting for end of track play\n");
        return false;
    }
    return true;
}

bool SpotiProxy::isPlaying()
{
    return m->track_playing;
}

int SpotiProxy::durationMs()
{
    return m->track_duration;
}

void SpotiProxy::stop()
{
    m->unloadTrack();
}

bool SpotiProxy::loginOk()
{
    return m->logged_in;
}

const string& SpotiProxy::getReason()
{
    return m->reason;
}


////////// NetFetch wrapper ////////////////////////////////////////////

// Forward decl
static FLAC__StreamEncoderWriteStatus writeCallback(
    const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void *client_data);


class SpotiFetch::Internal {
    static const int SAMPLES_BUF_SIZE = 16 * 1024;
public:

    Internal(SpotiFetch *parent)
        : p(parent) {
        encoder = FLAC__stream_encoder_new();
        if (nullptr == encoder) {
            LOGERR("FlacEncoder: Failed to construct stream encoder\n");
            return;
        }
    }

    ~Internal() {
        LOGDEB1("SpotiFetch::~SpotiFetch\n");
        SpotiProxy *spp = SpotiProxy::getSpotiProxy();
        if (nullptr == spp) {
            LOGERR("SpotiFetch::~SpotiFetch: getSpotiProxy returned null\n");
        } else {
            spp->stop();
        }
        if (encoder)
            FLAC__stream_encoder_delete(encoder);
    }
    

    // Write callback receiving data from Spotify (2/16/44100). We
    // send to Flac which in turns calls flacWriteCB()
    //
    // !! Hard-coded 2 channels of 16 bits samples. May have to change !!
    //
    int framesink(const void *frames, int num_frames, int channels, int rate) {
        LOGDEB1("SpotiFefch::framesink. num_frames " << num_frames <<
                " channels " << channels << " rate " << rate << endl);
        if (nullptr == encoder) {
            LOGERR("SpotiFetch::framesink: no encoder??\n");
            return -1;
        }

        // This can't be done in the constructor because init() will
        // write a 4 headers byte to the queue, which is only
        // initialized on start

        if (encoderneedinit) {
            encoderneedinit = false;
            FLAC__bool ok = true;
            ok &= FLAC__stream_encoder_set_compression_level(encoder, 0);
            ok &= FLAC__stream_encoder_set_channels(encoder, 2);
            ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, 16);
            ok &= FLAC__stream_encoder_set_sample_rate(encoder, 44100);
            if (!ok) {
                LOGERR("framesink: failed to set params for stream encoder\n");
                FLAC__stream_encoder_delete(encoder);
                encoder = nullptr;
                return -1;
            }

            FLAC__StreamEncoderInitStatus init_status =
                FLAC__stream_encoder_init_stream(
                    encoder, writeCallback, nullptr, nullptr, nullptr, this);
            if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
                LOGERR("framesink: failed to init stream encoder: "
                       << FLAC__StreamEncoderInitStatusString[init_status]
                       << endl);
                FLAC__stream_encoder_delete(encoder);
                encoder = nullptr;
                return -1;
            }
        }
        
        // A call with num_frames == 0 signals the end of stream
        if (num_frames == 0) {
            // Flush data
            FLAC__stream_encoder_finish(encoder);
            // Enqueue empty buffer.
            p->databufToQ(samplesBuf, 0);
            return 0;
        }
        
        int samples_left = 2 * num_frames;
        int16_t *smpp = (int16_t*)frames;
        while (samples_left > 0) {
            int samples = MIN(samples_left, SAMPLES_BUF_SIZE);
            // FLAC expects 32 bits signed integers
            for (int i = 0; i < samples; i++) {
                samplesBuf[i] = *smpp++;
            }
            if (!FLAC__stream_encoder_process_interleaved(
                    encoder, samplesBuf, samples / 2)) {
                LOGERR("FLAC__stream_encoder_process_interleaved failed!\n");
                return 0;
            }
            samples_left -= samples;
        }
        return num_frames;
    }

    // Flac data receiver: send to queue
    FLAC__StreamEncoderWriteStatus flacWriteCB(
        const FLAC__byte buffer[],
        size_t bytes, unsigned samples, unsigned current_frame) {
        p->databufToQ(buffer, bytes);
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    }

    FLAC__StreamEncoder *encoder{nullptr};
    bool encoderneedinit{true};
    FLAC__int32 samplesBuf[SAMPLES_BUF_SIZE];
    SpotiFetch *p;
};

bool SpotiFetch::reset()
{
    LOGDEB("SpotiFetch::reset\n");
    SpotiProxy *spp = SpotiProxy::getSpotiProxy();
    if (nullptr == spp) {
        LOGERR("SpotiFetch::start: getSpotiProxy returned null\n");
        return false;
    }
    spp->stop();
    m->encoderneedinit = true;
    return true;
}

static FLAC__StreamEncoderWriteStatus writeCallback(
    const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
    size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{
    SpotiFetch::Internal *o = (SpotiFetch::Internal *)client_data;
    return o->flacWriteCB(buffer, bytes, samples, current_frame);
}


SpotiFetch:: SpotiFetch(const std::string& url)
    : NetFetch(url), m(new Internal(this)) {}

SpotiFetch::~SpotiFetch() {}

bool SpotiFetch::start(BufXChange<ABuffer*> *queue, uint64_t offset)
{
    LOGDEB("SpotiFetch::start. queue " << queue << " queue name " <<
           (queue? queue->getname() : "null") << endl);
    SpotiProxy *spp = SpotiProxy::getSpotiProxy();
    if (nullptr == spp) {
        LOGERR("SpotiFetch::start: getSpotiProxy returned null\n");
        return false;
    }
    spp->stop();
    if (outqueue) {
        outqueue->setTerminate();
    }
    outqueue = queue;

    // For now lets say that rate is always 44100... Offset is used
    // with curl for byte-precise retrying of an interrupted
    // transfer. Well, we can't do it.
    int seekmsecs = offset / 44100;
    SpotiProxy::AudioSink sink =
        std::bind(&Internal::framesink, m.get(), _1, _2, _3, _4);
    // Note that the 'Uri' we get passed is actually just a trackId
    return spp->startPlay(_url, sink, seekmsecs);
}


bool SpotiFetch::waitForHeaders(int maxSecs)
{
    SpotiProxy *spp = SpotiProxy::getSpotiProxy();
    if (nullptr == spp) {
        LOGERR("SpotiFetch::start: getSpotiProxy returned null\n");
        return false;
    }
    // There is nothing to wait for, we're good after start
    LOGDEB("SpotiFetch::waitForHeaders: returning " << spp->isPlaying() <<endl);
    return spp->isPlaying();
}

bool SpotiFetch::headerValue(const std::string& nm, std::string& val)
{
    SpotiProxy *spp = SpotiProxy::getSpotiProxy();
    if (nullptr == spp) {
        LOGERR("SpotiFetch::start: getSpotiProxy returned null\n");
        return false;
    }
    if (!stringlowercmp("content-type", nm)) {
        val = "audio/flac";
        LOGDEB("SpotiFetch::headerValue: content-type: " << val << "\n");
        return true;
    } else if (!stringlowercmp("content-length", nm)) {
        uint64_t bytes = (spp->durationMs() / 10) * 441 * 4;
        ulltodecstr(bytes, val);
        LOGDEB("SpotiFetch::headerValue: content-length: " << val << "\n");
        return true;
    }
    return false;
}

bool SpotiFetch::fetchDone(FetchStatus *code, int *http_code)
{
    SpotiProxy *spp = SpotiProxy::getSpotiProxy();
    if (nullptr == spp) {
        LOGERR("SpotiFetch::start: getSpotiProxy returned null\n");
        return false;
    }
    LOGDEB("SpotiFetch::fetchDone: returning " << !spp->isPlaying() << endl);
    return !spp->isPlaying();
}
