#!/bin/bash

#_ips=( $(ifconfig | grep 'inet addr:' | cut -d: -f2 | cut -d " " -f1) )
#_devs=( $(ifconfig | egrep -o ".+ Link encap" | cut -d" " -f1) )
#ip route add table local broadcast 255.255.255.255 dev eth0 proto kernel scope link src $_ip
#echo "0" > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts
#total=${#_ips[@]}
#for (( i=1; i<=$total; i++ ))
#do
#	_dev=_devs[$i]
#	_ip=_ips[$i]
#	ifconfig $_dev -arp promisc up $_ip
#	ipfwadm -O -a deny -P all -S 0/0 -D 0/0 -W $_dev
#	ipfwadm -I -a deny -P all -S 0/0 -D 0/0 -W $_dev
#done

_dir="/home/tom/projects/cs219_bfr"
$_dir/bin/ccnfd -p 0.01 -v

sleep 0.5
$_dir/apps/scripts/experiment_flood.sh

