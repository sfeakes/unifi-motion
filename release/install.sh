#!/bin/bash

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SERVICE="unifi-motion"

BIN="unifi-motion"
CFG="unifi-motion.conf"
SRV="unifi-motion.service"
DEF="unifi-motion"

BINLocation="/usr/local/bin"
CFGLocation="/etc"
SRVLocation="/etc/systemd/system"
DEFLocation="/etc/default"

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

# Exit if we can't find systemctl
command -v systemctl >/dev/null 2>&1 || { echo "This script needs systemd's systemctl manager, Please check path or install manually" >&2; exit 1; }

# stop service, hide any error, as the service may not be installed yet
systemctl stop $SERVICE > /dev/null 2>&1

# copy files to locations, but only copy cfg if it doesn;t already exist

cp $BUILD/$BIN $BINLocation/$BIN
cp $BUILD/$SRV $SRVLocation/$SRV

if [ -f $CFGLocation/$CFG ]; then
  echo "Config exists, leaving alone! You may need to edit existing $CFGLocation/$CFG"
else
  cp $BUILD/$CFG $CFGLocation/$CFG
fi

if [ -f $DEFLocation/$DEF ]; then
  echo "Defaults exists, leaving alone! $DEFLocation/$DEF"
else
  cp $BUILD/$DEF.defaults $DEFLocation/$DEF
fi

systemctl enable $SERVICE
systemctl start $SERVICE
systemctl daemon-reload