#!/usr/bin/env perl

# Locate duplicate includes

# Copyright (c) 2005, Pavel Roskin
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.


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
