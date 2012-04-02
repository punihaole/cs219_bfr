#!/bin/sh

MYDIR=$(dirname $0)

$MYDIR/../bin/cstp -n /file/stream/random -d random_stream.bin $1
