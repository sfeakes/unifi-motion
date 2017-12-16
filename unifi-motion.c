
#include <libgen.h>
#include <netdb.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include "mongoose.h"

#define MAX_CAMERAS 10
#define JSON_MQTT_MSG_SIZE 100
#define LABEL_LEN 30

#define MAXCFGLINE 1024 
#define DELIM "="

#ifdef LOGGING_ENABLED
#define LOG(fmt, ...) log_message (fmt, ##__VA_ARGS__)
#else
#define LOG(...) {}
#endif

typedef int FD;

static char *_mqtt_address = "localhost:1883";
static char *_mqtt_user = NULL;
static char *_mqtt_passwd = NULL;
static char *_mqtt_pub_topic = "domoticz/in";
static char *_log_file_to_monitor = "/var/log/unifi-video/motion.log";
static char *_regexString = ".*type:(start|stop) .*\\((.*)\\) .*";

static char _mqtt_ID[20];

static bool _mqtt_exit_flag = false;

struct mg_connection *_mqtt_connection;

//static struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};

typedef struct 
{
   char name[LABEL_LEN]; // 100 character array
   char action[JSON_MQTT_MSG_SIZE]; // 100 character array
}motion;

motion *_motions[MAX_CAMERAS*2];

int _numMotions = 0;

void log_message(char *format, ...)
{
  va_list arglist;
  va_start( arglist, format );
  //vprintf( format, arglist );
  vfprintf(stderr, format, arglist );
  va_end( arglist );
}

bool addMotion(char *name, char *action) {

  if (_numMotions > MAX_CAMERAS*2)
    return false;

  _motions[_numMotions] = (motion *)malloc(sizeof(motion));

  strncpy(_motions[_numMotions]->name, name, LABEL_LEN);
  strncpy(_motions[_numMotions]->action, action, JSON_MQTT_MSG_SIZE);
  LOG("CFG Motion '%s'='%s'\n", name, action);
  _numMotions++;

  return true;
}

char *cleanwhitespace(char *str)
{
  char *end;
  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

char *cleanalloc(char*str)
{
  char *result;
  str = cleanwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
}

void readCfg (char *cfgFile)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  char *b_ptr;
  char *indx;

  if( (fp = fopen(cfgFile, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        // Eat leading whitespace
        while(isspace(*b_ptr)) b_ptr++;
        if ( b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          indx = strchr(b_ptr, '=');  
          if ( indx != NULL) 
          {
            if (strncasecmp (b_ptr, "mqtt_address", 12) == 0) {
              _mqtt_address = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_user", 9) == 0) {
              _mqtt_user = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_passwd", 11) == 0) {
              _mqtt_passwd = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_pub_topic", 13) == 0) {
              _mqtt_pub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_file", 8) == 0) {
              _log_file_to_monitor = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_regex", 9) == 0) {
              _regexString = cleanalloc(indx+1);
            } else {
              *indx = 0;
              addMotion(cleanwhitespace(b_ptr), cleanwhitespace(indx+1));
            }
          } 
        }
      }
    }
    fclose(fp);
  } else {
    fprintf(stderr, "ERROR reading config file '%s'\n%s\n", cfgFile,strerror (errno));
    exit (EXIT_FAILURE);
  }

  LOG("CFG mqtt address '%s'\n", _mqtt_address );
  LOG("CFG mqtt user '%s'\n", _mqtt_user );
  LOG("CFG mqtt passwd '%s'\n", _mqtt_passwd );
  LOG("CFG mqtt topic '%s'\n", _mqtt_pub_topic );
  LOG("CFG log to monitor topic '%s'\n", _log_file_to_monitor );
  LOG("CFG log regexp '%s'\n", _regexString );
}


bool mac(char *iface, char *buf, int len) {
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, iface);
  if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
    int i;
    for (i = 0; i < 6 && i * 2 < len; ++i) {
      // printf(":%02x", (unsigned char) s.ifr_addr.sa_data[i]);
      sprintf(&buf[i * 2], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
    }
    return true;
  }
  return false;
}

char *generate_mqtt_id(char *buf, int len) {
  extern char *__progname; // glibc populates this
  int i;
  strncpy(buf, basename(__progname), len);
  i = strlen(buf);

  if (i < len) {
    buf[i++] = '_';
    if (!mac("wlan0", &buf[i], len - i)) {
      mac("eth0", &buf[i], len - i);
    }
  }
  buf[len] = '\0';

  return buf;
}

void send_mqtt_msg(struct mg_connection *nc, char *message) {
  static uint16_t msg_id = 0;

  if ( _mqtt_exit_flag == true || _mqtt_connection == NULL) {
    fprintf(stderr, "ERROR: No mqtt connection, can't send message '%s'\n", message);
    return;
  }
  // basic counter to give each message a unique ID.
  if (msg_id >= 65535) {
    msg_id = 1;
  } else {
    msg_id++;
  }

  mg_mqtt_publish(nc, _mqtt_pub_topic, msg_id, MG_MQTT_QOS(0), message, strlen(message));

  LOG("MQTT: Published: '%s' with id %d\n", message, msg_id);

}

static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
  (void)nc;

  //if (ev != MG_EV_POLL) LOG("MQTT handler got event %d\n", ev);

  switch (ev) {
    case MG_EV_CONNECT: {
      _mqtt_exit_flag = false;
      //set_mqtt_cn(nc);
      struct mg_send_mqtt_handshake_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.user_name = _mqtt_user;
      opts.password = _mqtt_passwd;
      opts.keep_alive = 5;
      opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
      mg_set_protocol_mqtt(nc);
      mg_send_mqtt_handshake_opt(nc, _mqtt_ID, opts);
      fprintf(stderr, "Connected to mqtt %s with id of: %s\n", _mqtt_address, _mqtt_ID);
      _mqtt_connection = nc;
    } break;
    case MG_EV_MQTT_CONNACK:
      if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        LOG("Got mqtt connection error: %d\n", msg->connack_ret_code);
        _mqtt_exit_flag = true;
        _mqtt_connection = NULL;
      }
    break;
    case MG_EV_MQTT_PUBACK:
      LOG("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
    break;
    case MG_EV_CLOSE:
      fprintf( stderr, "MQTT Connection closed\n");
      _mqtt_exit_flag = true;
      _mqtt_connection = NULL;
    break;
  }
}


void start_mqtt (struct mg_mgr *mgr) {
  LOG("Starting MQTT service\n");

  if (mg_connect(mgr, _mqtt_address, ev_handler) == NULL) {
    fprintf(stderr, "mg_connect(%s) failed\n", _mqtt_address);
    exit(EXIT_FAILURE);
  }
  _mqtt_exit_flag = false;
}

FILE *open_log() {
  FILE *fd = NULL;

  fd = fopen(_log_file_to_monitor, "r");
  
  if (fd == NULL) {
    fprintf( stderr, "Open log file '%s' error\n%s\n",_log_file_to_monitor,strerror (errno));
    exit(EXIT_FAILURE);
  }
  
  if (fseek(fd, 0L, SEEK_END) != 0) {
    fprintf( stderr, "Seek to end of file log error\n%s\n",strerror (errno));
  }

  return fd;
}

void intHandler(int dummy) {
  int i;
  //keepRunning = 0;
  fprintf( stderr, "System exit, cleaning up\n");
  if (_mqtt_exit_flag != true)
    mg_mqtt_disconnect(_mqtt_connection);

  for (i=0; i<_numMotions; i++){
    free(_motions[i]);
  }
  free(_mqtt_address);
  free(_mqtt_user);
  free(_mqtt_passwd);
  free(_mqtt_pub_topic);
  free(_log_file_to_monitor);
  free(_regexString);

  exit(0);
}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  FILE *fd = NULL;
  char *line = NULL;
  char buf[LABEL_LEN];
  size_t len = 0;
  ssize_t read;
  int i;
  size_t maxGroups = 3;
  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  bool readConfig = false;
  bool logMotion = false;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0) {
      readCfg(argv[++i]);
      readConfig = true;
    } else if (strcmp(argv[i], "-lm") == 0) {
      logMotion = true;
    }
  }

  if (readConfig != true) {
    fprintf( stderr, "Error: Must pass config file on cmd line, example:-\n    %s -c %s.conf.\n",argv[0],argv[0]);
    exit(EXIT_FAILURE);
  }
  if (_numMotions <= 0) {
    fprintf( stderr, "Error: No motion actions found, please check config file.\n");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, intHandler);

  if (regcomp(&regexCompiled, _regexString, REG_EXTENDED)) {
    fprintf( stderr, "Error: Could not compile regular expression.\n");
    exit(EXIT_FAILURE);
  };

  generate_mqtt_id(_mqtt_ID, 20);

  mg_mgr_init(&mgr, NULL);

  LOG("Connecting to mqtt at '%s'\n", _mqtt_address );

  start_mqtt(&mgr);
  fd = open_log();

  fprintf(stderr, "Monotoring log %s, events to mqtt %s\n", _log_file_to_monitor, _mqtt_address);

  for (;;) {
    while (_mqtt_exit_flag == false && fd != NULL) {
      while ((read = getline(&line, &len, fd)) != -1) {
        LOG("Read from log:-\n  %s", line);
        if (regexec(&regexCompiled, line, maxGroups, groupArray, 0) == 0) {
          if (groupArray[2].rm_so == (size_t)-1) {
            LOG("No regexp from log file\n");
          } else {
            sprintf(buf, "%.*s:%.*s", (groupArray[1].rm_eo - groupArray[1].rm_so), (line + groupArray[1].rm_so), (groupArray[2].rm_eo - groupArray[2].rm_so),
                    (line + groupArray[2].rm_so));
            LOG("regexp from log file '%s'\n",buf);
            int i;
            for (i = 0; i < _numMotions; i++) {
              if (strcmp(_motions[i]->name, buf) == 0) {
                LOG("Motion match found\n");
                send_mqtt_msg(_mqtt_connection, _motions[i]->action);
                if (logMotion)
                  fprintf(stderr, "Motion seen %s\n",buf);
                break;
              }
            }
          }
        }
      }
      mg_mgr_poll(&mgr, 500);
      
    }
    sleep(1);
    LOG("Reset connections\n");
    if (_mqtt_exit_flag == true) {
      start_mqtt(&mgr);
    }
    if (fd == NULL) {
      fd = open_log();
    }
      
  }
  return 0;
}