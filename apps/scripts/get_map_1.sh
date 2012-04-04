#!/bin/sh

MYDIR=$(dirname $0)

#$MYDIR/../bin/cftp /flow1/map $MYDIR/../map1.jpg $1 > $MYDIR/../map1_out.txt 
$MYDIR/../bin/gchunk /flow1/map $1 > $MYDIR/../map1_out.txt 
