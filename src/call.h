#ifndef CALL_H
#define CALL_H

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <tox/tox.h>
#include <tox/toxav.h>
#include <sodium.h>

#include "suit.h"

/*******************************************************************************
 *
 * :: Controlling call test state machine
 *
 ******************************************************************************/
typedef enum CALLSTEP {
    /**
     * .
     */
    ENTERING,
    /**
     * .
     */
    GREETER,
    /**
     * .
     */
    RECORDING,
    /**
     * .
     */
    SAVING,
    /**
     * .
     */
    SENDING,
    ECHOING,
    /**
     * .
     */
    BYE,
} CALLSTEP;


/* should use struct SF_INFO */
struct record{
    int16_t *pcm;
    uint8_t channels;
    size_t samples_count;
    uint32_t samplerate;
};

#define CALL_TEST_DURATION 10 // in seconds
#define NUM_CALL_TESTERS 64 // TODO : add testers limit.
struct call_tester{
    struct list_head list;
    uint32_t friend_number;
    CALLSTEP step;    
    size_t samples_send;
    struct record *ctrecord; // should be dynamic ?
    struct record *playback;
    struct timespec tsprevious;
    struct timespec callstart;
    uint64_t delay;
    uint64_t elapsed;
    char *outfilename;
};

void calltest_init(ToxAV *toxav);
void calltest_destroy(ToxAV *toxav);

void call(ToxAV *toxAV, uint32_t friend_number, bool audio_enabled, bool video_enabled, void *user_data);
void call_state(ToxAV *toxAV, uint32_t friend_number, uint32_t state, void *user_data);
void audio_receive_frame(ToxAV *toxAV, uint32_t friend_number, const int16_t *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void *user_data);
void video_receive_frame(ToxAV *toxAV, uint32_t friend_number, uint16_t width, uint16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v, int32_t ystride, int32_t ustride, int32_t vstride, void *user_data);
void bit_rate_status_cb(ToxAV *toxAV, uint32_t friend_number, uint32_t audio_bit_rate,
                                      uint32_t video_bit_rate, void *user_data);
void calltest_iterate(struct suit_info *si, const struct timespec *tp);
void calltest_iterate_one_shot(struct suit_info *si, struct call_tester *ct, const struct timespec *tp);
uint64_t calltest_iteration_interval(void);

struct record *audio_load(const char * filename);
void audio_write(const char *destfile, const struct record *rec);
size_t audio_playback(ToxAV *toxAV, struct call_tester *ct, const struct record *rec);

#endif
