#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cstp -n /file/stream/random -s /dev/urandom $1
