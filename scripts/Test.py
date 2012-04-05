#!/usr/bin/python

import Loc

locFH = open("/tmp/pycore.51312/n1.xy")
x,y = Loc.readLoc(locFH)
print "(" + repr(x) + "," + repr(y) + ")\n"
