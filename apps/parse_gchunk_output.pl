#!/usr/bin/perl

use strict;
use warnings;

my $dir = shift;
my @filenames = `ls $dir/flow*.txt`;

foreach my $filename (@filenames) {
	open(FH, $filename) or die "Could not open $!\n";
	while (<FH>) {
		if ($_ =~ m/Took (\d+\.\d+)/g) {
			print "$1\n";
		}
	}
	close(FH);
}

