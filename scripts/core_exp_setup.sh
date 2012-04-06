#!/bin/bash

_ip=$(ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | cut -d " " -f1)
ip route add table local broadcast 255.255.255.255 dev eth0 proto kernel scope link src $_ip
echo "0" > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

_x=2000
_y=2000
_grid="$_x"'x'"$_y"

su tom
_dir="$HOME/projects/cs219_bfr"
echo $_dir
$_dir/bin/ccnud -p 0.01
sleep 0.5
$_dir/bin/bfrd -g $_grid

pycore=$(ls /tmp | egrep 'pycore\.[0-9]+' | cut -d. -f2)
_baseDir="/tmp/pycore.$pycore"

sleep 0.5
$_dir/apps/scripts/experiment_bfr.sh &

`python $_dir/scripts/NodeStart.py $_baseDir $_x $_y`
