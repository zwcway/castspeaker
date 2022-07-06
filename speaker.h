#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <string.h>
#include <common/package/pcm.h>

#include "common/common.h"
#include "common/audio.h"
#include "common/speaker_struct.h"


#define MAXLINE 80
#define BUFFER_LIST_SIZE 16

#include "common/audio.h"


enum output_type {
    OUTPUT_TYPE_RAW = 1,
    OUTPUT_TYPE_ALSA,
    OUTPUT_TYPE_PULSEAUDIO
};

extern pcm_header_t recv_buf[BUFFER_LIST_SIZE];
extern uint32_t ctrl_mtu;

void sexit(int no);

#endif
