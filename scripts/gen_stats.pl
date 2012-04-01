#!/usr/bin/perl

use strict;
use warnings;

sub parseLine($);

use constant {
START_STATE => 'START',
GOODPUT_STATE => 'GOODPUT',
OVERHEAD_STATE => 'OVERHEAD',
};

my @files = glob "sums/*_summary.txt";
my %goodput = ();
my %overhead = ();

my $state = START_STATE;
foreach (@files) {
	chomp;
	print "opening $_\n";
	open(FH, $_) or die print STDERR "Can't open file: $!\n";

	my $node = <FH>;

	while (my $line = <FH>) {
		if ($line =~ /^$/) {
			next;
		} elsif ($line =~ m/goodput/i) {
			$state = GOODPUT_STATE;
		} elsif ($line =~ m/overhead/i) {
			$state = OVERHEAD_STATE;
		} else {
			my ($t, $bytes) = parseLine($line);
			if ($t == -1) {
				print STDERR "invalid line: $line!\n";
				next;
			}
			if ($state eq GOODPUT_STATE) {
				if (defined $goodput{$t}) {
					$goodput{$t} += $bytes;
				} else {
					$goodput{$t} = $bytes;
				}
			} elsif ($state eq OVERHEAD_STATE) {
				if (defined $overhead{$t}) {
					$overhead{$t} += $bytes;
				} else {
					$overhead{$t} = $bytes;
				}
			} else {
				print STDERR "invalid state!\n";
				last;
			}
		}
	}
}

print "Summary:\n";
print "goodput:\n";
foreach (sort { $a<=>$b } keys %goodput) {
	print "$_: $goodput{$_} bytes/sec\n";
}

print "\noverhead:\n";
foreach (sort { $a<=>$b } keys %overhead) {
	print "$_: $overhead{$_} bytes/sec\n";
}

sub parseLine($)
{
	my $line = shift;
	if ($line =~ m/^(\d+): (\d+) bytes\/sec$/) {
		return ($1, $2);
	} else {
		return (-1, -1);
	}
}
