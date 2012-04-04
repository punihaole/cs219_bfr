#!/bin/sh

MYDIR=$(dirname $0)

#$MYDIR/../bin/cftp /flow2/map $MYDIR/../map2.jpg $1 > $MYDIR/../map2_out.txt
$MYDIR/../bin/gchunk /flow2/map $1 > $MYDIR/../map2_out.txt 
