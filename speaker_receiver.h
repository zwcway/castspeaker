/**
    This file is part of castspeaker
    Copyright (C) 2022-2028  zwcway

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#ifndef  SPEAKER_RECEIVER_H
#define  SPEAKER_RECEIVER_H

#include "speaker.h"

typedef int (*output_send_fn)(pcm_header_t *header, const uint8_t *data);

typedef int (*set_audio_format_fn)(audio_rate_t rate, audio_bits_t bits);

struct receiver_config {
    sa_family_t family;
    addr_t *ip;
    uint16_t port;
    output_send_fn output_cb;
    set_audio_format_fn format_cb;
};

int receiver_init(const struct receiver_config *cfg);

void receiver_deinit();

int receiver_start();

int receiver_stop();

#endif // SPEAKER_RECEIVER_H
