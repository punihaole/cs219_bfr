#!/usr/bin/perl

use strict;

# Takes an NS-2 mobility script and generates a CORE .imn file
# from the initial coordinates.
# Gives incorrect IP addresses if > 254 nodes
# ipaddresses are assigned from 2001:0::1 and 10.0.0.1
# You should open the imn file, create a WAP and link it to your nodes and save
# the imn file again, thus overwriting the boilerplate ip addr assignment.
# You also need this to compute and store the interfaces.

my $readfile = shift;

open(FHR, $readfile) or die print "Can't open file: $!\n";

my $lineno = 0;
while(1) {
	$lineno++;
	my $line = <FHR>;
	
	if ($line =~ m/^\#/) {
		# skip comments		
		next;
	}
	
	my $x;
	my $y;
	my $z;
	my $node;
	if ($line =~ m/^\$node_\((\d+)\) set X_ ([0-9.]+)/i) {
		# starting X position command
		$node = $1;
		$x = $2;
	} else {
		# must be end of starting coordinates, done
		last;	
	}
	
	$line = <FHR>;
	if ($line =~ m/^\$node_\((\d+)\) set Y_ ([0-9.]+)/i) {
		# starting Y position command
		$y = $2;
		
		if (!$1 eq $node) {
			print STDERR "Parse error, skipped $node Y coord!\n";
			exit(1);
		}
	} else {
		print STDERR "Parse error, expected to find Y coord at line $lineno\n";
		exit(1);
	}
	
	$line = <FHR>;	
	if ($line =~ m/^\$node_\((\d+)\) set Z_ ([0-9.]+)/i) {
		# starting Z position command
		$z = $2;
		
		if (!$1 eq $node) {
			print STDERR "Parse error, skipped $node Z coord!\n";
			exit(1);
		}
	} else {
		print STDERR "Parse error, expected to find Z coord at line $lineno\n";
		exit(1);
	}
	
	$node++;
	print "node n$node {\n";
	print "\ttype router\n";
	print "\tmodel ccn\n";
	print "\tnetwork-config {\n";
	print "\thostname n$node\n";
	print "\t!\n";
	print "\tinterface eth0\n";
	print "\t ipv6 address 2001:0::$node/128\n";
	print "\t ip address 10.0.0.$node/8\n";
	print "\t!\n";
	print "\t}\n";
	print "\tcanvas c1\n";
	print "\ticoncoords {$x $y}\n";
	print "\tlabelcoords {$x $y}\n";
	print "}\n\n";
}

print <<BOILERPLATE;
canvas c1 {
	name {Canvas1}
}

option global {
    interface_names no
    ip_addresses yes
    ipv6_addresses yes
    node_labels yes
    link_labels yes
    ipsec_configs yes
    exec_errors yes
    show_api no
    background_images no
    annotations yes
    grid yes
    traffic_start 0
}
BOILERPLATE

close(FHR);
exit(0);
