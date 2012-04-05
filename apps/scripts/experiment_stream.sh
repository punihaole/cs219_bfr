#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
INTERVAL=500

sleep 10

case "$HOST" in
	n25)
		$MYDIR/../bin/cstp -n /file/stream1/random -s /dev/urandom -i $INTERVAL $1
		;;
	n32)
		$MYDIR/../bin/cstp -n /file/stream2/random -s /dev/urandom -i $INTERVAL $1
		;;
	n11)
		$MYDIR/../bin/cstp -n /file/stream3/random -s /dev/urandom -i $INTERVAL $1
		;;
	n33)
		$MYDIR/../bin/cstp -n /file/stream4/random -s /dev/urandom -i $INTERVAL $1
		;;
	n34)
		$MYDIR/../bin/cstp -n /file/stream5/random -s /dev/urandom -i $INTERVAL $1
		;;
	n19)
		$MYDIR/../bin/cstp -n /file/stream6/random -s /dev/urandom -i $INTERVAL $1
		;;
	n43)
		$MYDIR/../bin/cstp -n /file/stream7/random -s /dev/urandom -i $INTERVAL $1
		;;
	n40)
		$MYDIR/../bin/cstp -n /file/stream8/random -s /dev/urandom -i $INTERVAL $1
		;;
	n41)
		$MYDIR/../bin/cstp -n /file/stream9/random -s /dev/urandom -i $INTERVAL $1
		;;
	n7) 
		$MYDIR/../bin/cstp -n /file/stream10/random -s /dev/urandom -i $INTERVAL $1
		;;
	n5)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream1/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream1.txt
		;;
	n9)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream2/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream2.txt
		;;
	n47)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream3/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream3.txt
		;;
	n22)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream4/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream4.txt
		;;
	n35)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream5/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream5.txt
		;;
	n6)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream6/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream6.txt
		;;
	n10)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream7/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream7.txt
		;;
	n24)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream8/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream8.txt
		;;
	n12)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream9/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream9.txt
		;;
	n36)
		sleep 20
		$MYDIR/../bin/cstp -n /file/stream10/random -d /dev/null -i $INTERVAL $1 > $MYDIR/../stream10.txt
		;;
	*)
		exit 0
esac
