#!/bin/sh

pycore=$(ls /tmp | grep pycore. | cut -d. -f2) 
_baseDir="/tmp/pycore.$pycore"

_dir="/home/tom/projects/cs219_ccnumr"

python $_dir/scripts/NodeStart.py $_baseDir 500 500
