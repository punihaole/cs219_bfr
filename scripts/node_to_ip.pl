#!/usr/bin/perl

use warnings;
use strict;

my $baseIp = 167772160; #10.0.0.0

for (my $i = 0; $i < @ARGV; $i=$i+1) {
	my $node = $ARGV[$i];
	my $ip = $baseIp + $node;
	printf "%-4s -> %d\n", $node, $ip;
	close(FH);
}
