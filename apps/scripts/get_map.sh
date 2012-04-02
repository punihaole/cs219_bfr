#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cftp /file/map map.jpg $1
