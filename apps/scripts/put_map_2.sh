#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /flow2/map $1
