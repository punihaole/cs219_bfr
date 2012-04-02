#!/bin/bash

if [ -z "$1" ]; then
	echo "Usage: $0 /path/to/output/dir"
	exit 1
fi

STAT_DIR=~/stat
OUTPUT_DIR=$1

if [ ! -d "$OUTPUT_DIR" ]; then
	echo "Directory $OUTPUT_DIR does not exist, trying to create..."
	mkdir -p $OUTPUT_DIR

	if [ ! -d "$OUTPUT_DIR" ]; then
		echo "Failed to create $OUTPUT_DIR"
		exit 1
	fi
	echo "Success"
fi

echo "Parsing files from $STAT_DIR"

NPROC=0
for file in `ls $STAT_DIR/*.stat`; do
	outfile=${file/%.stat/_summary.txt}
	outfile="${outfile##*/}"
	echo "Parsing $outfile"
	./parse_stats.pl $file 2>/dev/null 1>$OUTPUT_DIR/$outfile &
	NPROC=$(($NPROC+1))
	if [ "$NPROC" -ge 8 ]; then
		wait
		NPROC=0
	fi
done

wait
	
