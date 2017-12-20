
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "config.h"

bool addMotion(umConfig *config, char *name, char *action) {

  if (config->numMotions > MAX_CAMERAS*2)
    return false;

  config->motions[config->numMotions] = (umotion *)malloc(sizeof(umotion));

  strncpy(config->motions[config->numMotions]->name, name, LABEL_LEN);
  strncpy(config->motions[config->numMotions]->action, action, JSON_MQTT_MSG_SIZE);

  if (strlen(name) >= LABEL_LEN-1) {
    config->motions[config->numMotions]->name[LABEL_LEN-1] = 0;
    fprintf(stderr, "ERROR motion label '%s' is too long for buffer, truncated to '%s'\n", name, config->motions[config->numMotions]->name);
  }

  if (strlen(action) >= JSON_MQTT_MSG_SIZE-1) {
    config->motions[config->numMotions]->action[JSON_MQTT_MSG_SIZE-1] = 0;
    fprintf(stderr, "ERROR motion action '%s' is too long for buffer, truncated to '%s'\n", action, config->motions[config->numMotions]->action);
  }
  config->numMotions++;

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
  return result;
}

void parse_confg(umConfig *config, char *cfgFile)
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
              config->mqtt_address = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_user", 9) == 0) {
              config->mqtt_user = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_passwd", 11) == 0) {
              config->mqtt_passwd = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_pub_topic", 13) == 0) {
              config->mqtt_pub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_file", 8) == 0) {
              config->log_file_to_monitor = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_regex", 9) == 0) {
              config->regexString = cleanalloc(indx+1);
            } else {
              *indx = 0;
              addMotion(config, cleanwhitespace(b_ptr), cleanwhitespace(indx+1));
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

  int i;

  for (i=strlen(config->log_file_to_monitor); i > 0; i--) {
    if (config->log_file_to_monitor[i] == '/') {
      config->watchDir = malloc(sizeof(char) * (i+1));
      config->watchFile = malloc(sizeof(char) * ( (strlen(config->log_file_to_monitor)-i)+1 ) );
      strncpy(config->watchDir, config->log_file_to_monitor, i);
      strcpy(config->watchFile, &config->log_file_to_monitor[i+1]);
      config->watchDir[i] = '\0';
      break;
    }
  }
}

void print_config(umConfig *config)
{
  int i;

  fprintf(stderr, "CFG mqtt address '%s'\n", config->mqtt_address );
  fprintf(stderr, "CFG mqtt user '%s'\n", config->mqtt_user );
  fprintf(stderr, "CFG mqtt passwd '%s'\n", config->mqtt_passwd );
  fprintf(stderr, "CFG mqtt topic '%s'\n", config->mqtt_pub_topic );
  fprintf(stderr, "CFG log regexp '%s'\n", config->regexString );
  fprintf(stderr, "CFG log to monitor '%s'\n", config->log_file_to_monitor );
  fprintf(stderr, "CFG watch filename '%s'\n", config->watchFile );
  fprintf(stderr, "CFG watch directory '%s'\n", config->watchDir );

  for (i = 0; i < config->numMotions; i++) {
    fprintf(stderr, "CFG Motion '%s'='%s'\n", config->motions[i]->name, config->motions[i]->action);
  }
}

void free_config(umConfig *config)
{
  int i;

  for (i=0; i<config->numMotions; i++){
    free(config->motions[i]);
  }

  free(config->mqtt_address);
  free(config->mqtt_user);
  free(config->mqtt_passwd);
  free(config->mqtt_pub_topic);
  free(config->log_file_to_monitor);
  free(config->regexString);
  free(config->watchDir);
  free(config->watchFile);
}