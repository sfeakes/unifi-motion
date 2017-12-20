#ifndef CONFIG_H_
#define CONFIG_H_

#include <regex.h>

#define MAX_CAMERAS 20
#define JSON_MQTT_MSG_SIZE 100
#define LABEL_LEN 40
#define MQTT_ID_LEN 20

#define MAXCFGLINE 1024 


typedef struct {
   char name[LABEL_LEN];
   char action[JSON_MQTT_MSG_SIZE];
} umotion;

typedef struct {
  char *mqtt_address;
  char *mqtt_user;
  char *mqtt_passwd;
  char *mqtt_pub_topic;
  char *log_file_to_monitor;
  char *regexString;
  char mqtt_ID[MQTT_ID_LEN];
  char *watchDir;
  char *watchFile;
  regex_t regexCompiled;
  umotion *motions[MAX_CAMERAS*2];
  int numMotions;
} umConfig;


void parse_confg(umConfig *config, char *cfgFile);
void print_config();
void free_config(umConfig *config);

#endif