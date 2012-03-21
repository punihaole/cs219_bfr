#!/usr/bin/python

# This implements our user API to the routing daemon.
# Use this to implement the sendLoc message. This way we can use python scripts
# to update our location in CORE and in our routing.

import commands
import socket
import sys 
import os
import struct

ipCommand = 'ifconfig eth0 | grep "inet addr:" | cut -d: -f2 | cut -d " " -f1'

def getIp():
	_ip = commands.getoutput(ipCommand)
	octets = _ip.split(".")
	raw = int(octets[0]) << 24
	raw += int(octets[1]) << 16
	raw += int(octets[2]) << 8
	raw += int(octets[3])
	return raw
	
addr = '/tmp/bfr_' + repr(getIp()) + '.sock'
	
def sendLoc(x, y):
	sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
	print 'connecting to %s' % addr
	try:
		sock.connect(addr)
	except socket.error, msg:
		print >>sys.stderr, msg
		return -1
	buf = struct.pack('b', 0)
	sock.send(buf)
	buf = struct.pack('ii', 0, 16)
	sock.send(buf)
	buf = struct.pack('dd', x, y)
	sock.send(buf)
	rv = sock.recv(4)
	print "sending <" + repr(x) + "," + repr(y) + ">"
	sock.close()
	return rv

