#!/usr/bin/python

# Parses the n#.xy file for the x and y. Change of coordinates is done elsewhere

import sys
import os
import re

parser_re = re.compile(r'(\d+\.\d+) (\d+\.\d+)')

def readLoc(FH):
	line = FH.readline()
	m = parser_re.match(line);
	x = m.group(1)
	y = m.group(2)
	return float(x),float(y)
