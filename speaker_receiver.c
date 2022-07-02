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


#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <common/connection.h>
#include <common/event/select.h>
#include <common/package/control.h>
#include "common/speaker_struct.h"
#include "speaker.h"
#include "common/error.h"

#include "speaker_receiver.h"
#include "common/utils.h"
#include "speaker_multicast.h"

static uint16_t data_port = DEFAULT_RECEIVER_PORT;
static addr_t listen_ip = {AF_INET};
static output_send_fn output_fn = NULL;
static set_audio_format_fn format_fn = NULL;

static uint32_t ctrl_sample_chunk;
static audio_rate_t ctrl_sample_rate;
static audio_bits_t ctrl_sample_bits;

static connection_t conn = DEFAULT_CONNECTION_UDP_INIT;

LOG_TAG_DECLR("speaker");

void command(int fd, const control_package_t *hd) {
  switch (hd->cmd) {
    case SPCMD_CHUNK:
      ctrl_sample_chunk = hd->chunk.size;
      LOGI("command: chunk, %d", ctrl_sample_chunk);
      break;
    case SPCMD_SAMPLE:
      ctrl_sample_bits = hd->sample.bits;
      ctrl_sample_rate = hd->sample.rate;

      LOGI("command: sample, %d/%d/%s", rate_name(ctrl_sample_rate), bits_name(ctrl_sample_bits),
           channel_name(hd->sample.channel));

      if (NULL != format_fn) format_fn(ctrl_sample_rate, ctrl_sample_bits);

      break;
    case SPCMD_UNKNOWN_SP:
      bzero(&server_addr, sizeof(addr_t));
      break;
    default:
      LOGW("unknown command: %d", hd->cmd);
      break;
  }
}

int create_receiver_socket() {
  struct sockaddr_storage group_addr = {0};

  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    LOGF("create socket error: %m");
    exit(EERR_SOCKET);
  }

  set_sockaddr(&group_addr, &listen_ip, data_port);

  if ((bind(sockfd, (struct sockaddr *) &group_addr, sizeof(group_addr))) < 0) {
    LOGF("detect bind error: %m");
    exit(EERR_SOCKET);
  }

  return sockfd;
}

int sp_receiver_read(connection_t *c, const struct sockaddr_storage *src, socklen_t src_len, const void *package,
                     uint32_t len) {
  channel_header_t *hd = (channel_header_t *) package;
  uint8_t *samples = (uint8_t *) package + sizeof(channel_header_t);

  if (len == sizeof(control_package_t)) {
    command(c->read_fd, (const control_package_t *) package);
    return 0;
  }

  if (len - sizeof(channel_header_t) != hd->len) {
    LOGD("receiver recvfrom fail: %d(need %d)", len, hd->len);
    return -1;
  }

  LOGT("rate: %08d, bit: %03d, len: %05d", rate_name(hd->sample.rate), bits_name(hd->sample.bits), hd->len);

  if (output_fn && output_fn(hd, samples) != 0)
    return -1;

  uint8_t time_sync = 1;
  sendto(c->read_fd, &time_sync, sizeof(time_sync), 0, (struct sockaddr *) src, src_len);

  return 0;
}

int receiver_stop() {
  LOGD("exit receiver thread");
  event_del(&conn);

  shutdown(conn.read_fd, 0);
  close(conn.read_fd);

  return 0;
}

int receiver_start() {
  LOGT("receiver start");

  conn.read_fd = create_receiver_socket();
  event_add(&conn);

  return 0;
}

int receiver_init(const struct receiver_config *cfg) {
  LOGT("receiver init");
  if (cfg != NULL) {
    output_fn = cfg->output_cb;
    format_fn = cfg->format_cb;
    data_port = cfg->port;
    listen_ip.type = cfg->family;
    if (cfg->ip) memcpy(&listen_ip, cfg->ip, sizeof(addr_t));
    else bzero(&listen_ip.ipv6, sizeof(struct in6_addr));
  }

  if (!data_port) data_port = DEFAULT_RECEIVER_PORT;

  conn.read_cb = sp_receiver_read;

  receiver_start();

  return 0;
}

void receiver_deinit() {
  LOGT("receiver deinit");

  receiver_stop();
}
