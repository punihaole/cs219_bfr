#!/usr/bin/python

import Loc

locFH = open("/tmp/pycore.36851/n5.xy")
x,y = Loc.readLoc(locFH)
print "(" + repr(x) + "," + repr(y) + ")\n"
