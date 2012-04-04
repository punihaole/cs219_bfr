#!/bin/sh

MYDIR=$(dirname $0)

#$MYDIR/../bin/cftp /flow3/map $MYDIR/../map3.jpg $1 > $MYDIR/../map3_out.txt
$MYDIR/../bin/gchunk /flow3/map $1 > $MYDIR/../map3_out.txt 
