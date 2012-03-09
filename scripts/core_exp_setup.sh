#!/bin/sh

_ip=$(ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | cut -d " " -f1)
ip route add table local broadcast 255.255.255.255 dev eth0 proto kernel scope link src $_ip
echo "0" > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

_dir="/home/tom/projects/masters"

$_dir/bin/ccnud
time.sleep(1)
$_dir/bin/ccnumrd -g 1000x1000

_baseDir=$(cd /tmp && ls | grep pycore.)
_baseDir="/tmp/$_baseDir"

time.sleep(1)
python $_dir/scripts/NodeStart.py $_baseDir 1000 1000
