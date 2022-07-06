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
#include "speaker_receiver.h"
#include "common/utils.h"
#include "common/connection.h"
#include "common/event/select.h"
#include "common/event/udp.h"
#include "common/event/receive.h"
#include "common/package/control.h"
#include "common/package/detect.h"
#include "speaker_multicast.h"
#include "common/speaker_struct.h"
#include "speaker.h"
#include "common/error.h"

addr_t server_addr = {0};

static pthread_t multicast_thread;

static interface_t iface = {0};
static addr_t multicast_group = {0};
static uint16_t multicast_port = 0;
static detect_request_t header = {0};

static connection_t conn = DEFAULT_CONNECTION_UDP_INIT;

LOG_TAG_DECLR("speaker");

void save_server_info(detect_response_t *resp) {
  LOGI("server addr: %s", addr_ntop(&resp->addr));

  memcpy(&server_addr, &resp->addr, sizeof(addr_t));

  header.connected = DETECT_SERVER_CONNECTED;

  if (resp->type == DETECT_TYPE_EXIT) {
    LOGI("server exited");
  } else if (resp->type == DETECT_TYPE_FIRST_RUN) {
    LOGI("server runing");
  }
}

socket_t create_multicast_socket() {
  unsigned char multicastTTL = 1;
  socklen_t sock_len = 0;
  struct sockaddr_storage group_addr = {0};
  struct ipv6_mreq imreq = {0};
  int optlevel = 0, optname = 0, dont_loop = 0;
  sa_family_t af = iface.ip.type;
  addr_t addr = {.type = af, .ipv6 = IN6ADDR_ANY_INIT};

  socket_t cast_sockfd = socket(af, SOCK_DGRAM, IPPROTO_UDP);
  if (cast_sockfd < 0) {
    LOGF("create socket error: %m");
    sexit(EERR_SOCKET);
  }

  sock_len = set_sockaddr(&group_addr, &addr, multicast_port);

  if ((bind(cast_sockfd, (struct sockaddr *) &group_addr, sock_len)) < 0) {
    LOGF("detect bind error: %m");
    sexit(ERROR_SOCKET);
  }

  if (af == AF_INET) {
    optlevel = IPPROTO_IP;
    optname = IP_MULTICAST_TTL;
  } else if (af == AF_INET6) {
    optlevel = IPPROTO_IPV6;
    optname = IPV6_MULTICAST_HOPS;
  } else {
    LOGF("unsupport family %d", af);
    sexit(EERR_ARG);
  }
  if (setsockopt(cast_sockfd, optlevel, optname, (void *) &multicastTTL, sizeof(multicastTTL)) < 0) {
    LOGE("set option error: %m");
    closesocket(cast_sockfd);
    sexit(EERR_SOCKET);
  }

  if (af == AF_INET) {
    optname = IP_MULTICAST_IF;
    sock_len = sizeof(struct in_addr);
  } else {
    optname = IPV6_MULTICAST_IF;
    sock_len = sizeof(struct in6_addr);
  }
  if (setsockopt(cast_sockfd, optlevel, optname, (void *) &iface.ip.ipv6, sock_len) < 0) {
    LOGE("set option error: %m");
    closesocket(cast_sockfd);
    sexit(EERR_SOCKET);
  }

  if (af == AF_INET) {
    ((struct ip_mreq *) &imreq)->imr_interface = iface.ip.ipv4;
    ((struct ip_mreq *) &imreq)->imr_multiaddr = multicast_group.ipv4;
    sock_len = sizeof(struct ip_mreq);
    optname = IP_ADD_MEMBERSHIP;
  } else {
    ((struct ipv6_mreq *) &imreq)->ipv6mr_interface = iface.ifindex;
    ((struct ipv6_mreq *) &imreq)->ipv6mr_multiaddr = multicast_group.ipv6;
    sock_len = sizeof(struct ipv6_mreq);
    optname = IPV6_ADD_MEMBERSHIP;
  }
  if ((setsockopt(cast_sockfd, optlevel, optname, (const void *) &imreq, sock_len)) < 0) {
    LOGF("Failed add to multicast group: %m");
    sexit(ERROR_SOCKET);
  }


  if (af == AF_INET) {
    optname = IP_MULTICAST_LOOP;
  } else {
    optname = IPV6_MULTICAST_LOOP;
  }
  if (setsockopt(cast_sockfd, optlevel, optname, (void *) &dont_loop, sizeof(dont_loop)) < 0) {
    LOGF("setsockopt: %m");
    sexit(ERROR_SOCKET);
  }

  return cast_sockfd;
}

/**
 * 广播信息
 * @param arg
 * @return
 */
int sp_multicast_read(connection_t *c, const struct sockaddr_storage *src, socklen_t src_len, const void *package,
                      uint32_t len) {

  if (len != sizeof(detect_response_t)) {
    LOGD("recvfrom fail. need %d got %d from %s:%d", sizeof(detect_response_t), len, sockaddr_ntop(src),
         sockaddr_port(src));
    return -1;
  }

  save_server_info((detect_response_t *) package);

  return 0;
}

static void *thread_multicast(void *arg) {
  struct sockaddr_storage addr;
  ssize_t s;
  int timeout = 240;
  sa_family_t sf = iface.ip.type;
  uint8_t buffer[DETECT_REQUEST_SIZE(sf)];

  LOGI("speaker info %u (%s)%s:%d", header.id, mac_ntop(&header.mac), addr_ntop(&header.addr), header.data_port);

  set_sockaddr(&addr, &multicast_group, multicast_port);

  if (connect(conn.read_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    LOGE("connect error: %m");
  }
  while (!exit_thread_flag) {

    do {
      DETECT_REQUEST_ENCODE(sf, buffer, &header);
      s = sendto(conn.read_fd, (void *) &buffer, DETECT_REQUEST_SIZE(sf), 0, (struct sockaddr *) &addr, sizeof(addr));
      if (s < 0) {
        LOGE("sendto error: %m");
        break;;
      }
      if (s != DETECT_REQUEST_SIZE(sf)) {
        LOGD("wrong header size :%d need %d", s, DETECT_REQUEST_SIZE(sf));
        break;
      }
    } while (0);

    LOGD("multicast speaker info, size: %d", s);

    timeout = 12;
    while (timeout-- > 0) {
      sleep(5);
      if (!server_addr.type) {
        break;
      }
    }
  }
  pthread_exit(NULL);
}

int mcast_init(struct multicast_config *cfg) {
  LOGT("multicast init");

  if (cfg == NULL || NULL == cfg->iface) {
    sexit(EERR_ARG);
  }

  iface = *cfg->iface;
  if (cfg->multicast_group) {
    multicast_group = *cfg->multicast_group;
  } else {
    if (iface.ip.type == AF_INET) addr_stoa(&multicast_group, DEFAULT_MULTICAST_GROUP);
    else addr_stoa(&multicast_group, DEFAULT_MULTICAST_GROUPV6);
  }

  header.addr = iface.ip;
  header.mac = iface.mac;

  header.ver = 1;
  header.id = cfg->id;
  header.connected = DETECT_SERVER_DISCONECTED;
  header.data_port = cfg->data_port ? cfg->data_port : DEFAULT_RECEIVER_PORT;

  MASK_ARR_PACK(header.rate_mask, cfg->rate, RATEMASK_SIZE);
  MASK_ARR_PACK(header.bits_mask, cfg->bits, BITSMASK_SIZE);

  multicast_port = cfg->multicast_port ? cfg->multicast_port : DEFAULT_MULTICAST_PORT;

  conn.family = iface.ip.type;
  conn.read_cb = sp_multicast_read;
  conn.read_fd = create_multicast_socket();
  event_add(&conn);

  if (0 > pthread_create(&multicast_thread, NULL, thread_multicast, NULL)) {
    LOGE("multicast thread create error: %m");
    return -1;
  }
  pthread_detach(multicast_thread);

  return 0;
}

void mcast_deinit() {
  LOGT("multicast deinit");

  shutdown(conn.read_fd, 0);
  closesocket(conn.read_fd);

  pthread_cancel(multicast_thread);
}

