#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <signal.h>
#include <common/log.h>
#include <common/event/select.h>
#include <common/event/udp.h>
#include <common/event/receive.h>
#include "speaker.h"
#include "output/raw.h"
#include "common/error.h"
#include "common/speaker_struct.h"
#include "speaker_multicast.h"
#include "common/utils.h"
#include "speaker_receiver.h"
#include "output/raw.h"


#if PULSEAUDIO_ENABLE
#include "pulseaudio.h"
#endif

#if ALSA_ENABLE
#include "alsa.h"
#endif

#if PCAP_ENABLE
#include "pcap.h"
#endif

LOG_TAG_DECLR("speaker");


channel_header_t recv_buf[BUFFER_LIST_SIZE] = {0};

int exit_thread_flag = 0;
int verbosity = 0;

static speaker_id_t speaker_id = 0;

static char *sock_path = "/tmp/castspeaker.sock";
static char *config_file = "/etc/castspeaker/daemon.conf";
static char *ivshmem_device = NULL;
enum output_type output_mode = OUTPUT_TYPE_RAW;
static output_send_fn output_fn;
static char *alsa_device = "default";
static char *pa_sink = NULL;
static char *pa_stream_name = "Audio";
static interface_t interface = {0};

uint32_t gen_id() {
  return interface.ip.type > 0 ? interface.ip.ipv4.s_addr : random();
}

static void show_help(const char *arg0, int no) {
  printf("Usage: %s [-I <id>] [-p <port>] [-i <iface>] [-g <group>]\n", arg0);
  printf("\n");
  printf("         All command line options are optional. Default is to use\n");
  printf("         multicast with group address " DEFAULT_MULTICAST_GROUP ", port %d.\n", DEFAULT_MULTICAST_PORT);
  printf("\n");
  printf("         -I <id>                   : Use <id> as speaker id. Default is genrate\n");
  printf("                                     in auto.\n");
  printf("         -p <port>                 : Use <port> instead of default port %d.\n", DEFAULT_MULTICAST_PORT);
  printf("         -i <iface>                : Use local interface <iface>. Either the IP\n");
  printf("                                     or the interface name can be specified. \n");
  printf("                                     Uses this interface for IGMP.\n");
  printf("         -6                        : Use ipv6.\n");
  printf("         -g <group>                : Multicast group address.\n");
  printf("         -o pulse|alsa|raw         : Send audio to PulseAudio, ALSA or stdout.\n");
  printf("         -d <device>               : ALSA device name. 'default' if not specified.\n");
  printf("         -s <sink name>            : Pulseaudio sink name.\n");
  printf("         -n <stream name>          : Pulseaudio stream name/description.\n");
  printf("         -l <level>                : Log level. Default is 'info'.\n");
  printf("\n");
  exit(no);
}

void castspeaker_deinit();

void signal_handle(int signum) {
  LOGD("SIGNAL %d", signum);
  castspeaker_deinit();
  exit(0);
}

int init_receiver() {

  return 0;
}


int main(int argc, char *argv[]) {
  // Command line options
#ifndef ESP32
#if PULSEAUDIO_ENABLE
  output_mode = Pulseaudio;
#elif ALSA_ENABLE
  output_mode = Alsa;
#else
  output_mode = OUTPUT_TYPE_RAW;
#endif
#endif

  int opt, loglevel = -1, spid_isset = 0;
  char *interface_name = NULL;
  char *default_iface_name = NULL;
  char *group_ip = NULL;
  sa_family_t family = AF_INET;
  static addr_t multicast_group = {0};
  static uint16_t multicast_port = DEFAULT_MULTICAST_PORT;

  log_set_level(LOG_INFO);
  log_add_filter("queue", LOG_WARN);
  log_add_filter("event", LOG_WARN);

  while ((opt = getopt(argc, argv, "i:g:p:o:d:s:n:l:I:6h")) != -1) {
    switch (opt) {
      case 'l': // log level
        if (0 > log_set_level_from_string(optarg)) {
          printf("error log level: %s\n", optarg);
          show_help(argv[0], EERR_ARG);
        }
        break;
      case 'I':
        speaker_id = strtol(optarg, NULL, 10);
        spid_isset = 1;
        break;
      case 'i':
        if (strlen(optarg) > IF_NAMESIZE) {
          printf("Too long interface name '%s'\n", optarg);
          exit(EERR_ARG);
        }
        interface_name = strdup(optarg);
        break;
      case '6':
        family = AF_INET6;
        break;
      case 'p':
        multicast_port = strtol(optarg, NULL, 10);
        if (!multicast_port) show_help(argv[0], EERR_ARG);
        break;
      case 'g':
        if (0 != is_multicast_addr(optarg)) {
          printf("error multicast address: %s", optarg);
          show_help(argv[0], EERR_ARG);
        }
        group_ip = strdup(optarg);
        break;
      case 'o':
        if (strcmp(optarg, "pulse") == 0) output_mode = OUTPUT_TYPE_PULSEAUDIO;
        else if (strcmp(optarg, "alsa") == 0) output_mode = OUTPUT_TYPE_ALSA;
        else if (strcmp(optarg, "raw") == 0) output_mode = OUTPUT_TYPE_RAW;
        else {
          printf("error output mode: %s", optarg);
          show_help(argv[0], EERR_ARG);
        }
        break;
      case 'd':
//        alsa_device = strdup(optarg);
        break;
      case 's':
//        pa_sink = strdup(optarg);
        break;
      case 'n':
//        pa_stream_name = strdup(optarg);
        break;
      case 'h':
        show_help(argv[0], 0);
      default:
        show_help(argv[0], EERR_ARG);
    }
  }

  if (optind < argc) {
    LOGF("Expected argument after options");
    show_help(argv[0], EERR_ARG);
  }

  if (interface_name) {
    if (get_interface(family, &interface, interface_name) < 0) {
      printf("Invalid interface: %s\n", interface_name);
      interface_t list[16] = {0};
      int len = list_interfaces(family, list, 16);
      printf("Av interfaces:\n");
      for (int i = 0; i < len; ++i) {
        printf("%d. %s\n", i + 1, list[i].name);
      }
      printf("\n");
      exit(EERR_ARG);
    }
  } else {
    default_iface_name = malloc(IF_NAMESIZE);
    if (get_default_interface(family, default_iface_name) <= 0) {
      free(default_iface_name);
      LOGE("Get default interface failed");
      return -1;
    }
    if (get_interface(family, &interface, default_iface_name) < 0) {
      free(default_iface_name);
      printf("Get default interface failed. Please use -i <iface> and try again.\n");
      exit(EERR_ARG);
    }
    free(default_iface_name);
  }

  if (group_ip) {
    ip_stoa(&multicast_group, group_ip);
    free(group_ip);
    if (multicast_group.type != family) {
      printf("The multicast group ip family is ipv6, please use -6 and try again.\n");
      show_help(argv[0], EERR_ARG);
    }
  }

  speaker_id = spid_isset ? speaker_id : gen_id();
  LOGI("speaker id: %u", speaker_id);

  signal(SIGINT, signal_handle);
  signal(SIGKILL, signal_handle);
  signal(SIGSEGV, signal_handle);
  signal(SIGTERM, signal_handle);
  signal(SIGQUIT, signal_handle);

  LOGI("Starting receiver");

  // initialize output
  switch (output_mode) {
    case OUTPUT_TYPE_PULSEAUDIO:
      printf("Pulseaudio not support yet.\n");
      exit(EERR_ARG);
    case OUTPUT_TYPE_ALSA:
      printf("ALSA not support yet.\n");
      exit(EERR_ARG);
    case OUTPUT_TYPE_RAW:
      printf("Using raw output\n");
      if (raw_output_init() != 0) {
        printf("Raw output init failed.\n");
        exit(EERR_ARG);
      }
      output_fn = raw_output_send;
    default:
      break;
  }

  event_init(EVENT_TYPE_SELECT, EVENT_PROTOCOL_UDP, 4096, 100);

  // init receiver

  struct receiver_config receiver_cfg = {
    .family = family,
    .ip = interface_name ? &interface.ip : NULL,
    .port = 0,
    .output_cb = output_fn,
  };
  receiver_init(&receiver_cfg);

  struct multicast_config multicast_cfg = {
    .id = speaker_id,
    .multicast_group = multicast_group.type ? &multicast_group : NULL,
    .interface = &interface,
    .multicast_port = multicast_port,
    .data_port = 0,
    .rate = {RATE_44100, RATE_48000},
    .bits = {BIT_16, BIT_24, BIT_32},
  };
  mcast_init(&multicast_cfg);

  if (interface_name) free(interface_name);

  while (!exit_thread_flag) {
    event_start();
  }

  castspeaker_deinit();
  return 0;
}

void castspeaker_deinit() {
  LOGT("speaker deinit");

  exit_thread_flag = 1;

  event_deinit();

  receiver_deinit();
  mcast_deinit();
}

void sexit(int no) {
  castspeaker_deinit();
  exit(no);
}
