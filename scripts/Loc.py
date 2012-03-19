#!/usr/bin/python

# Parses the n#.xy file for the x and y. Change of coordinates is done elsewhere

import sys
import os

def readLoc(FH):
	line = FH.readline()
	line = line.strip()
	words = line.split()
	x = words[0]
	y = words[1]
	return float(x),float(y)
