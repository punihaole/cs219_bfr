#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cftp /flow4/map $MYDIR/../map4.jpg $1 > $MYDIR/../map4_out.txt
