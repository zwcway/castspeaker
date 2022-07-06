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


#include "raw.h"


static header_sample_t ro_data = {0};
static int rate, bits;
static char *channel;

LOG_TAG_DECLR("output");

int raw_output_init() {
  return 0;
}

int raw_output_send(pcm_header_t *header, const uint8_t *data) {
  header_sample_t *hs = &header->sample;

  if (memcmp(&ro_data, hs, sizeof(header_sample_t)) != 0) {
    ro_data = *hs;

    rate = rate_name(hs->rate);
    bits = bits_name(hs->bits);
    if (bits == 0) {
      LOGE("Unsupported sample size %d, not playing until next format switch.\n", hs->bits);
    }
    channel = channel_name(hs->channel);
    LOGI("Channel is %s\n", channel);
  }

  if (!hs->rate) return 1;

  //fwrite(data->audio, 1, data->audio_size, stdout);
  LOGI("bits:%02d rate:%d ch:%s\r", bits, rate, channel);

  return 0;
}
