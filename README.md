# UniFi-Motion

## Detect motion from unifi-video and post to a MQTT broker

Exceptionally quick and dirty first virsion.  Will come back and clean up later.

This will monitior a log file and if particular strings are matched will post a message to a MQTT broker.
The config is set for unifi-video and domoticz (with a MQTT broker installed), but can be modified to work with most systems.

This depends on Mongoose https://github.com/cesanta/mongoose, I've just copied the mongoose.c and mongoose.h files to this repository (read quick & dirty comment)

Standard unifi-video systems you should just be able to copy the release directory to you system and run `install.sh`.
Other systems copy everything and use make to compile.

This depends on systemd / systemctl to run as a service or daemon, if you have init.d system then you'll have't make your own `/etc/init.d/unifi-motion` script, but it shouldn't be too difficult.

## configuration options :-
Edit `/etc/unifi-motion.conf`

### Logfile to monitor

```
log_file = /var/log/unifi-video/motion.log
```

### MQTT broker information
```
mqtt_address = localhost:1883
mqtt_user = someusername    
mqtt_passwd = somepassword
mqtt_pub_topic = domoticz/in
```

### What to match in the monitored logfile and the action to take.
For unifi-video motion detection the example below is best.
format is :-
start or stop : camera name = message to post to MQTT broker.

```
start:Front Door = {"idx":105,"nvalue":1,"svalue":""}  
stop:Front Door = {"idx":105,"nvalue":0,"svalue":""}

start:Back Door = {"idx":106,"nvalue":1,"svalue":""}  
stop:Back Door = {"idx":106,"nvalue":0,"svalue":""}

start:Driveway = {"idx":107,"nvalue":1,"svalue":""}  
stop:Driveway = {"idx":107,"nvalue":0,"svalue":""}

start:Back Garden = {"idx":108,"nvalue":1,"svalue":""}  
stop:Back Garden = {"idx":108,"nvalue":0,"svalue":""}
```


### Regular Expression to use within the monitored log file.
This is set for unifi-video motion.log, if that's what you are monitoring, leave it alone.

```
log_regex = .*type:(start|stop) .*\((.*)\) .*
```

For custom log file monotoring, here are some details on how to use the regular expression.

The regexp is expressed in POSIX regular expression format, and MUST have two groups. The output from those two groups will be used to match entries in this config file. The group output is merged together with the : character.

So in the default config used here, unifi-video motion.log output is similar to :-
 ```
 1513373389.823 2017-12-15 15:29:49.823/CST: INFO Camera[000000000000] type:stop event:10386 clock:3286698801 (Front Door) in app-event-bus-1
 ```

the regexp will look for characters that match either `start` or `stop` after the word `type:` and before the next space. That's group 1.
Next the regexp will look for any text within () and that's group 2 output.
Those two values are commbined with `:` character and search the configuration file for the appropiate action. 
This example `stop:Front Door` would be matched and `{"idx":105,"nvalue":1,"svalue":""}` would be sent to the MQTT broker that's configured.

Here is a good online regex testing utililty.
https://regex101.com


# Other options
For the sake of speed and size, all logging information is stripped at compile time, only one flag exists for runtime (-lm explained below), if that's not enough you'll need to recompile with options listed below.
 
## Service configuration options

edit `/etc/defaults/unifi-motion` only option to change is `-lm`, that stands for Log Motion, with the flag matched items from the log file will get recorded in syslog, without it the process is silent except for errors & warnings.

```
OPTS= -c /etc/unifi-motion.conf -lm
```

## Compile options

Edit Makefile and uncomment one for the 3 for how much crap you want to know.

```
LOGGING =
LOGGING = -D LOGGING_ENABLED
LOGGING = -D DEBUG_LOGGING_ENABLED
```


# Home Automation options

The example coniguration is set to Domoticz.  Domoticz expects JSON over MQTT and uses IDX for the sensor identifyer and nvaule or 1 or 0 for on or off.  svalue is not needed, but I left it for any custom messages you may want to add.
`{"idx":105,"nvalue":1,"svalue":""}`
So in domoticz, just create a virtual motion censor, one for each camera, get the IDX and use that in the configuration.

All other home automation hubs should be similar, and all have MQTT plug-ins / add-ons, but your on your own as to the exact configuration.


## License

See the [LICENSE](LICENSE.md) file for license rights and limitations (MIT).
