#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /file/map $1
