#!/usr/bin/perl

while (<>) {
	s/^\d+: //g;
	s/bytes\/sec$//g;
	print "$_";
}
