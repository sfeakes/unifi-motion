
#include <libgen.h>
#include <netdb.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <net/if.h>


#include <unistd.h>
#include <sys/poll.h>

#include "mongoose.h"
#include "config.h"


#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#define LOGFILE_OPEN_TRIES 5

#ifdef DEBUG_LOGGING_ENABLED
  #define DLOG(fmt, ...) log_message (fmt, ##__VA_ARGS__)
  #define LOGGING_ENABLED
#else
  #define DLOG(...) {}
#endif

#ifdef LOGGING_ENABLED
  #define LOG(fmt, ...) log_message (fmt, ##__VA_ARGS__)
#else
  #define LOG(...) {}
#endif

typedef struct {
  FILE *logfile;
  int trigger;
  int descriptor;
} umwHandles;

typedef enum {mqttstarting, mqttrunning, mqttstopped} mqttstatus;
static umwHandles _umHandles;
static mqttstatus _mqtt_status = mqttstopped;
struct mg_connection *_mqtt_connection;

static umConfig _config;

void cleanup();

void log_message(char *format, ...)
{
  va_list arglist;
  va_start( arglist, format );
  vfprintf(stderr, format, arglist );
  va_end( arglist );
}

 // Find the first network interface with valid MAC and put mac address into buffer upto length
bool mac(char *buf, int len)
{
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct if_nameindex *if_nidxs, *intf;

  if_nidxs = if_nameindex();
  if (if_nidxs != NULL)
  {
    for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++)
    {
      strcpy(s.ifr_name, intf->if_name);
      if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
      {
        int i;
        if ( s.ifr_addr.sa_data[0] == 0 &&
             s.ifr_addr.sa_data[1] == 0 &&
             s.ifr_addr.sa_data[2] == 0 &&
             s.ifr_addr.sa_data[3] == 0 &&
             s.ifr_addr.sa_data[4] == 0 &&
             s.ifr_addr.sa_data[5] == 0 ) {
          continue;
        }
        for (i = 0; i < 6 && i * 2 < len; ++i)
        {
          sprintf(&buf[i * 2], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
        }
        return true;
      }
    }
  }

  return false;
}

// Need to update network interface.
char *generate_mqtt_id(char *buf, int len) {
  extern char *__progname; // glibc populates this
  int i;
  strncpy(buf, basename(__progname), len);
  i = strlen(buf);

  if (i < len) {
    buf[i++] = '_';
    // If we can't get MAC to pad mqtt id then use PID
    if (!mac(&buf[i], len - i)) {
      sprintf(&buf[i], "%.*d", (len-i), getpid());
    }
  }
  buf[len] = '\0';

  return buf;
}

void send_mqtt_msg(struct mg_connection *nc, char *message) {
  static uint16_t msg_id = 0;

  if ( _mqtt_status == mqttstopped || _mqtt_connection == NULL) {
  //if ( _mqtt_exit_flag == true || _mqtt_connection == NULL) {
    fprintf(stderr, "ERROR: No mqtt connection, can't send message '%s'\n", message);
    return;
  }
  // basic counter to give each message a unique ID.
  if (msg_id >= 65535) {
    msg_id = 1;
  } else {
    msg_id++;
  }

  mg_mqtt_publish(nc, _config.mqtt_pub_topic, msg_id, MG_MQTT_QOS(0), message, strlen(message));

  LOG("MQTT: Published: '%s' with id %d\n", message, msg_id);

}

static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
  (void)nc;

  //if (ev != MG_EV_POLL) LOG("MQTT handler got event %d\n", ev);
  switch (ev) {
    case MG_EV_CONNECT: {
      struct mg_send_mqtt_handshake_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.user_name = _config.mqtt_user;
      opts.password = _config.mqtt_passwd;
      opts.keep_alive = 5;
      opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
      mg_set_protocol_mqtt(nc);
      mg_send_mqtt_handshake_opt(nc, _config.mqtt_ID, opts);
      fprintf(stderr, "Connected to mqtt %s with id of: %s\n", _config.mqtt_address, _config.mqtt_ID);
      _mqtt_status = mqttrunning;
      _mqtt_connection = nc;
    } break;
    case MG_EV_MQTT_CONNACK:
      if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        LOG("Got mqtt connection error: %d\n", msg->connack_ret_code);
        _mqtt_status = mqttstopped;
        _mqtt_connection = NULL;
      }
    break;
    case MG_EV_MQTT_PUBACK:
      LOG("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
    break;
    case MG_EV_CLOSE:
      fprintf( stderr, "MQTT Connection closed\n");
      _mqtt_status = mqttstopped;
      _mqtt_connection = NULL;
    break;
  }
}


void start_mqtt (struct mg_mgr *mgr) {
  LOG("Starting MQTT service\n");

  if (mg_connect(mgr, _config.mqtt_address, ev_handler) == NULL) {
    fprintf(stderr, "mg_connect(%s) failed\n", _config.mqtt_address);
    exit(EXIT_FAILURE);
  }
  _mqtt_status = mqttstarting;
}

void close_log(FILE *log) {
  if (log != NULL) {
    fclose(log);
  } else if (_umHandles.logfile != NULL) {
    fclose(_umHandles.logfile);
  }
}

FILE *open_log(bool seekBackOneLine)
{
  FILE *fp = NULL;
  int i=0;

  // Try to open log file a number of times, with pause inbetween.  Do this for logroatate
  // as the file may get moved and not re-created for a small period of time.
  while (fp == NULL)
  {
    if ( NULL != (fp = fopen(_config.log_file_to_monitor, "r")))
      break;
    
    i++;
    fprintf(stderr, "Open log file '%s' error\n%s\n", _config.log_file_to_monitor, strerror(errno));
    
    if (i > LOGFILE_OPEN_TRIES) {
      fprintf(stderr, "ERROR: Can't open logfile '%s', giving up\n", _config.log_file_to_monitor);
      cleanup();
      exit(EXIT_FAILURE);
    }
    sleep(1);
  }

  if (seekBackOneLine == false)
  {
    if (fseek(fp, 0L, SEEK_END) != 0)
    {
      fprintf(stderr, "Seek to end of file log error\n%s\n", strerror(errno));
    }
  }
  else
  {
    char c;
    fseek(fp, -1, SEEK_END); //next to last char, last is EOF
    c = fgetc(fp);
    
    while (c == '\n') //define macro EOL
    {
      if (fseek(fp, -2, SEEK_CUR) != 0){break;}
      c = fgetc(fp);
    }
    while (c != '\n')
    {
      if (fseek(fp, -2, SEEK_CUR) != 0){break;}
      c = fgetc(fp);
    }
    if (c == '\n')
      fseek(fp, 1, SEEK_CUR);
  }

  _umHandles.logfile = fp;
  return fp;
}

void cleanup() {
  close_log(_umHandles.logfile);
  inotify_rm_watch(_umHandles.trigger, _umHandles.descriptor);

  if (_mqtt_status == mqttrunning && _mqtt_connection != NULL)
    mg_mqtt_disconnect(_mqtt_connection);

  free_config(&_config);
}

void intHandler(int dummy) {
  
  fprintf( stderr, "System exit, cleaning up\n");
  cleanup();
  exit(0);
}

int action_log_changes(FILE *fp, bool logMotion) {
  char *line = NULL;
  size_t len = 0;
  ssize_t read_size;
  size_t maxGroups = 3;
  regmatch_t groupArray[maxGroups];
  static char buf[LABEL_LEN];
  int lc = 0;

  while ((read_size = getline(&line, &len, fp)) != -1) {
    DLOG("Read from log:-\n  %s", line);
    lc++;
    if (regexec(&_config.regexCompiled, line, maxGroups, groupArray, 0) == 0) {
      if (groupArray[2].rm_so == (size_t)-1) {
        DLOG("No regexp from log file\n");
      } else {
        sprintf(buf, "%.*s:%.*s", (groupArray[1].rm_eo - groupArray[1].rm_so), (line + groupArray[1].rm_so), (groupArray[2].rm_eo - groupArray[2].rm_so),
                (line + groupArray[2].rm_so));
        LOG("regexp from log file '%s'\n", buf);
        int i;
        for (i = 0; i < _config.numMotions; i++) {
          if (strcmp(_config.motions[i]->name, buf) == 0) {
            DLOG("Motion match found\n");
            send_mqtt_msg(_mqtt_connection, _config.motions[i]->action);
            if (logMotion)
              fprintf(stderr, "Motion seen %s\n", buf);
            break;
          }
        }
      }
    }
  }

  return lc;
}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  //umConfig config;
  int i;
  bool readConfig = false;
  char buffer[BUF_LEN];
  int length;
  int retval;
  bool logMotion = false;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0) {
      parse_confg(&_config, argv[++i]);
      readConfig = true;
    } else if (strcmp(argv[i], "-lm") == 0) {
      logMotion = true;
    }
  }

  if (readConfig != true) {
    fprintf(stderr, "Error: Must pass config file on cmd line, example:-\n    %s -c %s.conf.\n", argv[0], argv[0]);
    exit(EXIT_FAILURE);
  }
  if (_config.numMotions <= 0) {
    fprintf(stderr, "Error: No motion actions found, please check config file.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

#ifdef LOGGING_ENABLED
  print_config(&_config);
#endif

  signal(SIGINT, intHandler);

  if (regcomp(&_config.regexCompiled, _config.regexString, REG_EXTENDED)) {
    fprintf(stderr, "Error: Could not compile regular expression. %s\n", strerror(errno));
    cleanup();
    exit(EXIT_FAILURE);
  };

  generate_mqtt_id(_config.mqtt_ID, MQTT_ID_LEN);

  mg_mgr_init(&mgr, NULL);
  LOG("Connecting to mqtt at '%s'\n", _config.mqtt_address);
  start_mqtt(&mgr);

  _umHandles.trigger = inotify_init();
  if (_umHandles.trigger < 0) {
    fprintf(stderr, "ERROR Failed to register monitor with kernel. %s\n", strerror(errno));
    cleanup();
    exit(EXIT_FAILURE);
  }

  _umHandles.descriptor = inotify_add_watch(_umHandles.trigger, _config.watchDir, IN_MODIFY);
  _umHandles.logfile = open_log(false);

  // Set watch trigger for non-blocking poll
  struct pollfd fds[1];
  fds[0].fd = _umHandles.trigger;
  fds[0].events = POLLIN;

  fprintf(stderr, "Monotoring log %s, events to mqtt %s\n", _config.log_file_to_monitor, _config.mqtt_address);

  while (1) {
    int i = 0;

    if ( _mqtt_status == mqttstopped) {
      LOG("Reset connections\n");
      // Should put a pause in here after first try failed.
      // Not putting it in yet untill debugged mqtt random disconnects.
      start_mqtt(&mgr);
    }
    mg_mgr_poll(&mgr, 0);

    retval = poll(fds, 1, 1000);
    if (retval == -1)
      fprintf(stderr, "Warning, system call poll() failed");
    else if (!retval) { // trigger timeout.
      DLOG(".");
      continue;
    } else { // trigger file action.
      DLOG(":");
    }

    length = read(_umHandles.trigger, buffer, BUF_LEN);

    if (length < 0) {
      fprintf(stderr, "Warning system call read() failed");
    } else if (length == 0) {
      fprintf(stderr, "Warning system call read() returned blank");
    }

    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len) {
        if (event->mask & IN_MODIFY) {
          if (!(event->mask & IN_ISDIR)) {
            // LOG("The file %s was modified.\n", event->name );
            if (strcmp(event->name, _config.watchFile) == 0) {
              if (action_log_changes(_umHandles.logfile, logMotion) <= 0) {
                fprintf(stderr, "No lines read, logfile may have been rotated, re-opening:-\n");
                close_log(_umHandles.logfile);
                _umHandles.logfile = open_log(true);
                action_log_changes(_umHandles.logfile, logMotion);
              }
            }
          }
        }
      }
      i += EVENT_SIZE + event->len;
    }
  }

  cleanup();
  
}