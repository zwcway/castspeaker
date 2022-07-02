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


#ifndef MULTICAST_H
#define MULTICAST_H

#include <netinet/in.h>
#include "common/castspeaker.h"
#include "common/audio.h"
#include "speaker.h"


struct multicast_config {
    speaker_id_t id;
    addr_t *multicast_group;
    interface_t *interface;
    uint16_t multicast_port;
    uint16_t data_port;
    audio_rate_t rate[RATEMASK_SIZE];
    audio_bits_t bits[BITSMASK_SIZE];
};

extern addr_t server_addr;


int mcast_init(struct multicast_config *cfg);

void mcast_deinit();


#endif
