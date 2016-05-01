/*    Copyright (C) 2016  Ronan Bignaux
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "call.h"
#include "file.h"
#include "list.h"
#include "timespec.h"

#include "ylog/ylog.h"
#include <sndfile.h>

/* Design :
 *
 * we fix an audio length = 20ms
 * and try to get call every 20ms...
 *
 *  960 samples @ 48khz = 20 ms
 *
 */
#define AUDIO_LENGTH 20 // in millisecond , IMPROVE : dynamic
#define DEFAULT_DELAY AUDIO_LENGTH * NSEC_PER_MSEC

static struct list_head calltesters;
static char * greeterfilename = "greeter.wav";
static struct record *greeter;

struct call_tester *call_tester_by_friend_number(struct list_head *call_testers, const uint32_t friend_number);
void call_tester_destroy(struct call_tester *calltester);
void call_testers_destroy(struct list_head *calltesters);

void record_free(struct record *rec)
{
    if(!rec) return;
    free(rec->pcm);
    free(rec);
}

void record_init(struct record *rec)
{
    rec->pcm = NULL;
    rec->samples_count = 0;
}

void calltest_init(ToxAV *toxav)
{
    INIT_LIST_HEAD(&calltesters);

    greeter = audio_load(greeterfilename);

    toxav_callback_call(toxav, call, NULL);
    toxav_callback_call_state(toxav, call_state, NULL);
    toxav_callback_audio_receive_frame(toxav, audio_receive_frame, NULL);
    toxav_callback_video_receive_frame(toxav, video_receive_frame, NULL);
    toxav_callback_bit_rate_status(toxav, bit_rate_status_cb, NULL);
}

void calltest_destroy(ToxAV *toxav)
{
    call_testers_destroy(&calltesters);

    toxav_callback_call(toxav, NULL, NULL);
    toxav_callback_call_state(toxav, NULL, NULL);
    toxav_callback_audio_receive_frame(toxav, NULL, NULL);
    toxav_callback_video_receive_frame(toxav, NULL, NULL);
    toxav_callback_bit_rate_status(toxav, NULL, NULL);
}

struct call_tester *call_tester_add_entry(struct list_head *call_testers, const uint32_t friend_number)
{
    struct list_head *ptr;
    struct call_tester *calltester;
    struct call_tester *entry;

    // debug
    calltester = call_tester_by_friend_number(call_testers, friend_number);
    if(calltester) {
        yerr("calltester already exists for friend %d",calltester->friend_number);
        return calltester;
    }

    calltester = malloc(sizeof(struct call_tester));
    calltester->ctrecord = malloc(sizeof(struct record));
    record_init(calltester->ctrecord);

    calltester->playback = NULL;
    calltester->friend_number = friend_number;
    calltester->step = ENTERING;
    calltester->elapsed = 0;
    calltester->delay = DEFAULT_DELAY; // UINT64_MAX;
    calltester->outfilename = strdup("/tmp/call-XXXXXX.wav");
    int fd = mkstemps(calltester->outfilename, 4);
    close(fd);

    list_for_each(ptr, call_testers) {
        entry = list_entry(ptr, struct call_tester, list);
        if (entry->friend_number > calltester->friend_number) {
            list_add_tail(&calltester->list, ptr);
            return calltester;
        }
    }
    list_add_tail(&calltester->list, call_testers);
    return calltester;
}


void call_tester_destroy(struct call_tester *calltester)
{
    list_del(&calltester->list);
    record_free(calltester->ctrecord);
    record_free(calltester->playback);
    free(calltester);
}

void call_testers_destroy(struct list_head *calltesters)
{
    struct call_tester *f;

    while( !list_empty(calltesters) ) {
        f = list_entry(calltesters->next,struct call_tester,list);
        call_tester_destroy(f);
    }
}

struct call_tester *call_tester_by_friend_number(struct list_head *call_testers, const uint32_t friend_number)
{
    struct call_tester *ct;

    list_for_each_entry(ct, call_testers, list) {
        if (ct->friend_number == friend_number)
        {
            return ct;
        }
    }
    return NULL;
}

void call(ToxAV *toxAV, uint32_t friend_number, bool audio_enabled, bool video_enabled, void *user_data)
{
    TOXAV_ERR_ANSWER err;

    toxav_answer(toxAV, friend_number, audio_enabled ? AUDIO_BITRATE : 0, video_enabled ? VIDEO_BITRATE : 0, &err);

    if (err != TOXAV_ERR_ANSWER_OK) {
        yerr("Could not answer call, friend: %d, error: %d", friend_number, err);
    }
    call_tester_add_entry(&calltesters, friend_number);
}

// TODO : incomplete, not detecting mute friend , => IMPROVE
void call_state(ToxAV *toxAV, uint32_t friend_number, uint32_t state, void *user_data)
{
    struct call_tester *ct;
    ydebug("Call state for friend %d changed to %d", friend_number, state);
    if (state & TOXAV_FRIEND_CALL_STATE_ERROR) {
        yerr("Call with friend %d error %d", friend_number, state);
        return;
    } else if (state & TOXAV_FRIEND_CALL_STATE_SENDING_A) {
        // TODO : add TOXAV_FRIEND_CALL_STATE_SENDING_V case
        bool send_audio = (state & TOXAV_FRIEND_CALL_STATE_SENDING_A) && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);
        bool send_video = (state & TOXAV_FRIEND_CALL_STATE_SENDING_V) && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V);
        toxav_bit_rate_set(toxAV, friend_number, send_audio ? AUDIO_BITRATE : 0, send_video ? VIDEO_BITRATE : 0, NULL);
        ydebug("Call state for friend %d changed to %d: audio: %d, video: %d", friend_number, state, send_audio, send_video);
        call_tester_add_entry(&calltesters, friend_number);
        return;
    } else if (state & TOXAV_FRIEND_CALL_STATE_FINISHED) {
        ct =  call_tester_by_friend_number(&calltesters, friend_number);
        if(!ct) {
            ywarn("TOXAV_FRIEND_CALL_STATE_FINISHED and no caller found.");
            return;
        }
        ct->step = BYE;
        return;
    }
}

void bit_rate_status_cb(ToxAV *toxAV, uint32_t friend_number, uint32_t audio_bit_rate,
                        uint32_t video_bit_rate, void *user_data)
{
    /* Just accept what toxav wants the bitrate to be... */
    //    if (v_bitrate > (uint32_t)UTOX_MIN_BITRATE_VIDEO) {
    ////        TOXAV_ERR_BIT_RATE_SET error = 0;
    ////        toxav_bit_rate_set(AV, f_num, -1, v_bitrate, &error);
    //        if (error) {
    //            debug("ToxAV:\tSetting new Video bitrate has failed with error #%u\n", error);
    //        } else {
    //            debug("uToxAV:\t\tVideo bitrate changed to %u\n", v_bitrate);
    //        }
    //    } else {
    //        debug("uToxAV:\t\tVideo bitrate unchanged %u is less than %u\n", v_bitrate, UTOX_MIN_BITRATE_VIDEO);
    //    }
    yinfo("bit_rate_status_cb : audio %u , video %u", audio_bit_rate, video_bit_rate);
    return;
}


void audio_receive_frame(ToxAV *toxAV, uint32_t friend_number, const int16_t *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void *user_data)
{
    struct call_tester *ct = call_tester_by_friend_number(&calltesters, friend_number);
    struct record *prec;

    // would never happens
    if (!ct) {
        yerr("call audio_receive_frame() , ct = NULL");
        return;
    }

    // ignore audio outside RECORDING step is needed by callback mecanism.
    if (ct->step != RECORDING) {
        ytrace("call audio_receive_frame() and state != RECORDING : %d",ct->step);
        return;
    }

    // TODO : emulate skip/zero rt comm
    prec = ct->ctrecord;
    prec->channels = channels; // move it
    prec->samplerate = sampling_rate;

    // TODO : Manage sampling_rate / channels changes ??
    //    if ( prec->sampling_rate != sampling_rate )
    //        ydebug("sampling_rate has changed");
    //= time(NULL)

    ytrace("Entering RECORDING session, ctrecord->sample_count = %zu", prec->samples_count);

    prec->pcm = realloc(prec->pcm, sizeof(int16_t) * channels * (sample_count + prec->samples_count));
    memcpy(&prec->pcm[channels * prec->samples_count], pcm, sizeof(int16_t) *  channels * sample_count);
    prec->samples_count += sample_count;
}

// do nothing, just print info.
bool jitter(struct call_tester *ct, const struct timespec *tp)
{
    struct timespec *tp1 = &ct->tsprevious;
    ct->elapsed  += ((tp->tv_sec - tp1->tv_sec) * NSEC_PER_SEC) + tp->tv_nsec - tp1->tv_nsec;
    ytrace("tp1.tv_sec = %ju , tp1.tv_nsec = %ld", tp1->tv_sec, tp1->tv_nsec);
    ytrace(" tp.tv_sec = %ju ,  tp.tv_nsec = %ld , ct->elapsed  = %ld ", tp->tv_sec, tp->tv_nsec, ct->elapsed );

    ct->elapsed = 0;
    memcpy(&ct->tsprevious,tp, sizeof(struct timespec));
    return true;
}

/**
 * Return the time in milliseconds before calltest_iterate() should be called again
 * for optimal performance.
 * => the littlest delay .
 *
 */
uint64_t calltest_iteration_interval(void)
{
    struct call_tester *ct;
    uint64_t ldelay = MAX_NS;

    list_for_each_entry(ct, &calltesters, list)
    {
        ldelay = ct->delay > ldelay ? ldelay : ct->delay;
    }
    return ldelay;
}

void calltest_iterate(struct suit_info *si, const struct timespec *tp)
{
    struct call_tester *ct, *n;
    list_for_each_entry_safe(ct, n, &calltesters, list)
            calltest_iterate_one_shot(si, ct, tp);
}

size_t audio_playback(ToxAV *toxAV, struct call_tester *ct, const struct record *rec)
{
    TOXAV_ERR_SEND_FRAME errframe;
    size_t sample_count = (rec->samplerate * AUDIO_LENGTH) / 1000 ;
    ct->delay = ( sample_count * 1000 ) / rec->samplerate * NSEC_PER_MSEC;
    //    ct->delay -= 1 * NSEC_PER_MSEC;
    //    if(!jitter(ct, tp))
    //        return;

    if (ct->samples_send < rec->samples_count ) {
        // seem to be blocking
        toxav_audio_send_frame(toxAV, ct->friend_number, &rec->pcm[ct->samples_send * rec->channels],
                sample_count, rec->channels, rec->samplerate, &errframe);

        if (errframe != TOXAV_ERR_SEND_FRAME_OK) {
            ydebug("Could not send audio frame to friend: %d, error: %d", ct->friend_number, errframe);
            return 0;
        }
        ct->samples_send += sample_count;
        return sample_count;
    } else return 0;
}

// not async ready but loaded one time at soft init.
struct record *audio_load(const char * filename)
{
    sf_count_t sf_readf = 0;
    struct record *rec = NULL;
    SF_INFO sfinfo;

    SNDFILE *sndfile = sf_open(filename, SFM_READ, &sfinfo);

    if (!sndfile) {
        ydebug("Not able to open output file : %s, error : %s.", filename, sf_strerror(NULL));
        sf_close(sndfile);
        return NULL;
    }
    rec = malloc(sizeof(struct record));
    record_init(rec);

    rec->samplerate = sfinfo.samplerate;
    rec->channels = sfinfo.channels;

    rec->pcm = malloc(sizeof(int16_t) * sfinfo.frames * sfinfo.channels);
    sf_readf = sf_readf_short(sndfile, rec->pcm, sfinfo.frames);
    rec->samples_count = sf_readf;

    if(sf_close(sndfile)) {
        yerr("%s", sf_strerror(NULL));
    }
    return rec;
}

// TODO : rewrite non-blocking
void audio_write(const char *destfile, const struct record *rec)
{
    SNDFILE *outfile;
    SF_INFO sfinfo = {
        .samplerate	= rec->samplerate,
        .channels	= rec->channels,
        .format		= SF_FORMAT_WAV | SF_FORMAT_PCM_16
    };

    if (!(outfile = sf_open(destfile, SFM_WRITE, &sfinfo))) {
        ydebug("Not able to open output file %s.", destfile);
        yerr("%s", sf_strerror(NULL));
        return;
    }

    sf_write_short(outfile, rec->pcm, rec->samples_count * rec->channels);
    sf_close(outfile);
}

void calltest_iterate_one_shot(struct suit_info *si, struct call_tester *ct, const struct timespec *tp)
{
    TOXAV_ERR_CALL_CONTROL errcall;

    switch (ct->step) {
    case ENTERING:
        ytrace("Entering ENTERING");
        ct->playback = malloc(sizeof(struct record));
        record_init(ct->playback);
        ct->samples_send = 0;
        toxav_call_control(si->toxav, ct->friend_number, TOXAV_CALL_CONTROL_MUTE_AUDIO, &errcall);
        ytrace("Entering GREETER");
        ct->step++;
        //        break;

    case GREETER:
        // TODO ignore audio TOXAV_CALL_CONTROL_UNMUTE_AUDIO
        if (!audio_playback(si->toxav, ct, greeter))
        {
            memcpy(&ct->callstart,tp,sizeof(struct timespec)); // IMPROVE : for next step.
            toxav_call_control(si->toxav, ct->friend_number, TOXAV_CALL_CONTROL_UNMUTE_AUDIO, &errcall);
            ytrace("Entering RECORDING");
            ct->step++;
        }
        break;

    case RECORDING:

        if (nanosec_timeout(&ct->callstart, tp, CALL_TEST_DURATION * NSEC_PER_SEC))
        {
            // mute friend to save bandwich and cpu
            toxav_call_control(si->toxav, ct->friend_number, TOXAV_CALL_CONTROL_MUTE_AUDIO, &errcall);
            ct->step++;
        }
        break;

    case SAVING:
        ytrace("Entering SAVING sampling_rate=%d", ct->ctrecord->samplerate);
        audio_write(ct->outfilename, ct->ctrecord);
        ct->samples_send = 0;
        ct->step++;
        break;

    case SENDING:
        add_filesender(si->tox, ct->friend_number, ct->outfilename);
        ytrace("Entering SENDING %s to %d", ct->outfilename, ct->friend_number);
        ct->step++;
        break;

    case ECHOING:
        ytrace("Entering ECHOING, sending : %zu/%zu", ct->samples_send, ct->ctrecord->samples_count);
        if(!audio_playback(si->toxav, ct, ct->ctrecord))
            ct->step++;
        break;

    case BYE:

        ytrace("Entering BYE");
        toxav_call_control(si->toxav,  ct->friend_number, TOXAV_CALL_CONTROL_CANCEL, &errcall);
        if (errcall != TOXAV_ERR_CALL_CONTROL_OK)
            ydebug("%d", errcall);

        call_tester_destroy(ct);
        ytrace("Quitting BYE");
        break;
    }
}

void video_receive_frame(ToxAV *toxAV, uint32_t friend_number, uint16_t width, uint16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v, int32_t ystride, int32_t ustride, int32_t vstride, void *user_data)
{
    ystride = abs(ystride);
    ustride = abs(ustride);
    vstride = abs(vstride);

    if (ystride < width || ustride < width / 2 || vstride < width / 2) {
        ywarn("video_receive_frame error");
        return;
    }

    uint8_t *y_dest = (uint8_t*)malloc(width * height);
    uint8_t *u_dest = (uint8_t*)malloc(width * height / 2);
    uint8_t *v_dest = (uint8_t*)malloc(width * height / 2);

    for (size_t h = 0; h < height; h++) {
        memcpy(&y_dest[h * width], &y[h * ystride], width);
    }

    for (size_t h = 0; h < height / 2; h++) {
        memcpy(&u_dest[h * width / 2], &u[h * ustride], width / 2);
        memcpy(&v_dest[h * width / 2], &v[h * vstride], width / 2);
    }

    TOXAV_ERR_SEND_FRAME err;
    toxav_video_send_frame(toxAV, friend_number, width, height, y_dest, u_dest, v_dest, &err);

    free(y_dest);
    free(u_dest);
    free(v_dest);

    if (err != TOXAV_ERR_SEND_FRAME_OK) {
        yinfo("Could not send video frame to friend: %d, error: %d", friend_number, err);
    }
}
