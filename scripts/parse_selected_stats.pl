#!/usr/bin/perl

use warnings;
use strict;

if (@ARGV < 2) {
	print "Usage ./parse_selected_stats.pl summary_directory node(s)\n";
	exit 1;
}

my $dir = shift;
my $baseIp = 167772160; #10.0.0.0

for (my $i = 1; $i < @ARGV; $i=$i+1) {
	my $node = $ARGV[$i];
	my $ip = $baseIp + $node;
	my @ls_out = `ls $dir/*$ip\_summary.txt`;
	my $filename = $ls_out[0];
	open(FH, $filename) or die "Could not open $!.\n";
	
	while (<FH>) {
		print "$_";
	}
	print "-----------------------------------------\n";
	close(FH);
}
