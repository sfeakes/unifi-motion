# unifi-motion
Detect motion from unifi-video and post to a MQTT broker

Exceptionally quick and dirty first virsion.  Will come back and clean up later.

This will monitior a log file and if particular strings are matched will post a message to a MQTT broker.
The config is set for unifi-video and domoticz (with MQTT installed), but can be modified to work with most systems.

This depends on Mongoose https://github.com/cesanta/mongoose, I've just copied the mongoose.c and mongoose.h files to this repository (read quick & dirty comment)

Standard unifi-video systems you should just be able to copy the release directory to you system and run install.sh.
Other systems copy everything and run make.

This depends on systemd / systemctl to run as a service or daemon, if you have init.d system then you'll have t make your own /etc/init.d/unifi-motion script, but it shouldn't be too difficult.

See the unifi-video.conf (under release) for information on configuration.
