#!/bin/bash

_x=2000
_y=2000
_grid="$_x"'x'"$_y"

_dir="$HOME/projects/cs219_bfr"
echo $_dir
$_dir/bin/ccnud -p 0.01
sleep 0.5
echo "$_grid"
$_dir/bin/bfrd -g $_grid

pycore=$(ls /tmp | egrep 'pycore\.[0-9]+' | cut -d. -f2)
_baseDir="/tmp/pycore.$pycore"

sleep 0.5
../apps/scripts/experiment_bfr.sh &

`python $_dir/scripts/NodeStart.py $_baseDir $_x $_y`
