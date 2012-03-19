#!/bin/sh

_ip=$(ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | cut -d " " -f1)
ip route add table local broadcast 255.255.255.255 dev eth0 proto kernel scope link src $_ip
echo "0" > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

_dir="/home/tom/projects/cs219_ccnumr"

$_dir/bin/ccnud -p 0.01
$_dir/bin/ccnumrd -g 500x500

pycore=$(ls /tmp | grep pycore. | cut -d. -f2)
_baseDir="/tmp/pycore.$pycore"

sleep 0.5
`python $_dir/scripts/NodeStart.py $_baseDir 500 500`
