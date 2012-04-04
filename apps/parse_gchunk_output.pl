#!/usr/bin/perl

my $filename = shift;

open(FH, $filename) or die "Could not open $!\n";

while (<FH>) {
	if ($_ =~ m/Took (\d+\.\d+)/g) {
		print "$1\n";
	}
}

close(FH);

