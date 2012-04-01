#!/usr/bin/perl

use strict;
use warnings;
use DateTime::Format::Strptime;
use Date::Parse;
use Math::BigInt;

# Takes a bfrd/ccnud/ccnfd generated .stat file and parses.

sub parseLine($);
sub dateToStamp($);

my $filename = shift;

open(FH, $filename) or die print STDERR "Can't open file: $!\n";

my $currentSec = -1;

my $line = <FH>;
my ($rv, $day, $month, $date, $hour, $min, $sec, $year, $node, $event, $size) = parseLine($line);
if ($rv != 0) {
	print STDERR "could not parse first line!\n";
	exit 1;
}

my $str = "$date/$month/$year $hour:$min:$sec";
my $currentTime = dateToStamp($str);
my $lineNo = 1;
my $startTime = $currentTime;
my %goodput = ();
my %overhead = ();
if ($event =~ m/DATA_RCVD/) {
	$goodput{0} = $size;
} else {
	$overhead{0} = $size;
}

while($line = <FH>) {
	($rv, $day, $month, $date, $hour, $min, $sec, $year, $node, $event, $size) = parseLine($line);
	if ($rv != 0) {
		print STDERR "could not parse line $lineNo.\n";
		print STDERR "$line\n";
		next;
	}
	
	my $str = "$date/$month/$year $hour:$min:$sec";
	my $timestamp = dateToStamp($str);
	my $t = $timestamp - $startTime;

	if ($event =~ m/DATA_RCVD/) {
		if (defined $goodput{$t}) {
		$goodput{$t} += $size;
		} else {
			$goodput{$t} = $size;
		}
	} else {
		if (defined $overhead{$t}) {
			$overhead{$t} += $size;
		} else {
			$overhead{$t} = $size;
		}
	}

	$lineNo++;
}
close(FH);

print "$node\n";
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
	if ($line =~ m/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) (\d{1,2}) (\d+):(\d+):(\d+) (\d{4}) \w+_stats_(\d+): EVENT ([A-Za-z_]+) (\d+)$/) {
		return (0, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
	} else {
		return (1, "", "", "", "", "", "", "", "", "", "");
	}
}

sub dateToStamp($)
{
	my $date = shift;
	my $parser = DateTime::Format::Strptime->new(pattern => '%d/%b/%Y %H:%M:%S');
	my $timestamp = $parser->parse_datetime($date);
	return str2time($date);
}
