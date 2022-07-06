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
#include <pthread.h>
#include <common/connection.h>
#include <common/event/select.h>
#include <common/package/control.h>
#include "common/speaker_struct.h"
#include "speaker.h"
#include "common/error.h"

#include "speaker_receiver.h"
#include "speaker_multicast.h"

static uint16_t data_port = DEFAULT_RECEIVER_PORT;
static addr_t listen_ip = {AF_INET};
static output_send_fn output_fn = NULL;
static set_audio_format_fn format_fn = NULL;

static uint32_t ctrl_sample_chunk;
static audio_rate_t ctrl_sample_rate;
static audio_bits_t ctrl_sample_bits;
static pcm_header_t pcm_header = {0};

static connection_t conn = DEFAULT_CONNECTION_UDP_INIT;

LOG_TAG_DECLR("speaker");

void command(socket_t fd, const void *hd) {
  control_package_t ctl;
  CONTROL_PACKAGE_DECODE(&ctl, hd);

  switch (ctl.cmd) {
    case SPCMD_CHUNK:
      ctrl_sample_chunk = ctl.chunk.size;
      LOGI("command: chunk, %d", ctrl_sample_chunk);
      break;
    case SPCMD_SAMPLE:
      ctrl_sample_bits = ctl.sample.bits;
      ctrl_sample_rate = ctl.sample.rate;

      LOGI("command: sample, %d/%d/%s", rate_name(ctrl_sample_rate), bits_name(ctrl_sample_bits),
           channel_name(ctl.sample.channel));

      if (NULL != format_fn) format_fn(ctrl_sample_rate, ctrl_sample_bits);

      break;
    case SPCMD_UNKNOWN_SP:
      memset(&server_addr, 0, sizeof(addr_t));
      break;
    default:
      LOGW("unknown command: %d", ctl.cmd);
      break;
  }
}

void pcm_receive() {

}

socket_t create_receiver_socket() {
  struct sockaddr_storage group_addr = {0};

  socket_t sockfd = socket(listen_ip.type, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    LOGF("create socket error: %m");
    sexit(EERR_SOCKET);
  }

  set_sockaddr(&group_addr, &listen_ip, data_port);

  LOGI("Listen on %s", addr_ntop(&listen_ip));

  if ((bind(sockfd, (struct sockaddr *) &group_addr, sizeof(group_addr))) < 0) {
    LOGF("detect bind error: %m");
    closesocket(sockfd);
    sexit(EERR_SOCKET);
  }

  return sockfd;
}

int sp_receiver_read(connection_t *c, const struct sockaddr_storage *src, socklen_t src_len, const void *package,
                     uint32_t len) {
  uint8_t *samples = (uint8_t *) package + PCM_HEADER_SIZE;

  if (len == CONTROL_PACKAGE_SIZE) {
    command(c->read_fd, package);
    return 0;
  }

  PCM_HEADER_DECODE(&pcm_header, package);

  if (len - PCM_HEADER_SIZE != pcm_header.len) {
    LOGD("receiver recvfrom fail: %d(need %d)", len, pcm_header.len);
    return -1;
  }

  LOGT("rate: %08d, bit: %03d, len: %05d", rate_name(pcm_header.sample.rate), bits_name(pcm_header.sample.bits),
       pcm_header.len);

  if (output_fn && output_fn(&pcm_header, samples) != 0)
    return -1;

  uint8_t time_sync = 1;
  sendto(c->read_fd, &time_sync, sizeof(time_sync), 0, (struct sockaddr *) src, src_len);

  return 0;
}

int receiver_stop() {
  LOGD("exit receiver thread");
  event_del(&conn);

  shutdown(conn.read_fd, 0);
  closesocket(conn.read_fd);

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
  if (cfg == NULL || 0 == cfg->family) {
    LOGF("family can not empty");
    sexit(EERR_ARG);
  }
  listen_ip.type = cfg->family;
  if (cfg->output_cb != NULL) output_fn = cfg->output_cb;
  if (cfg->format_cb != NULL) format_fn = cfg->format_cb;
  if (cfg->port) data_port = cfg->port;
  if (cfg->ip) listen_ip = *cfg->ip;
  else memset(&listen_ip.ipv6, 0, sizeof(struct in6_addr));

  if (!data_port) data_port = DEFAULT_RECEIVER_PORT;

  conn.family = cfg->family;
  conn.read_cb = sp_receiver_read;

  receiver_start();

  return 0;
}

void receiver_deinit() {
  LOGT("receiver deinit");

  receiver_stop();
}
