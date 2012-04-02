#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /flow1/map $1
