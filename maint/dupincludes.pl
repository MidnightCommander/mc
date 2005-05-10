#!/usr/bin/env perl

# Copyright (c) 2005, Pavel Roskin
# This script is Free Software, and it can be copied, distributed and
# modified under the terms of GNU General Public License, version 2.

# Locate duplicate includes

use strict;

my %sys_includes;
my %loc_includes;

if ($#ARGV != 0) {
	print "Usage: dupincludes.pl file\n";
	exit 1;
}

my $filename = $ARGV[0];

if (!open (FILE, "$filename")) {
	print "Cannot open file \"$filename\"\n";
	exit 1;
}

foreach (<FILE>) {
	if (/^\s*#\s*include\s*<(.*)>/) {
		if (defined $sys_includes{$1}) {
			print "$filename: duplicate <$1>\n";
		} else {
			$sys_includes{$1} = 1;
		}
	} elsif (/^\s*#\s*include\s*"(.*)"/) {
		if (defined $loc_includes{$1}) {
			print "$filename: duplicate \"$1\"\n";
		} else {
			$loc_includes{$1} = 1;
		}
	}
}
