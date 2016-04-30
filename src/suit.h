#ifndef _SUIT_H
#define _SUIT_H

#include "friend.h"
#include "misc.h"

#include <tox/tox.h>
#include <tox/toxav.h>

// in kb/s
#define AUDIO_BITRATE  48
#define VIDEO_BITRATE  5000

struct suit_info {
    Tox *tox;
    ToxAV *toxav;
    char *data_filename;
    uint64_t last_purge;
    uint64_t start_time;
    char *version;

    struct client_info own_info;
    struct list_head friends_info;
};

#endif
