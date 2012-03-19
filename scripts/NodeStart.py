#!/usr/bin/python

from socket import gethostname
import os
import sys
import time

import Loc
import Ccnumr

baseDir = sys.argv[1]
print "baseDir: " + repr(baseDir)
height = float(sys.argv[2])
width = float(sys.argv[3])
node = gethostname()

# the file we will read our x,y coords from
locFilename = baseDir + "/" + node + ".xy"
print "Reading loc file: " + repr(locFilename)

while True:
		locFH = open(locFilename, "r")
		x, y = Loc.readLoc(locFH)
		y = height - y
		print repr(node) + " sending location update: <" + repr(x) + "," + repr(y) + ">\n"
		Ccnumr.sendLoc(x, y)
		locFH.close()
		time.sleep(10)
