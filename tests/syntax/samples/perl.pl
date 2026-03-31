#!/usr/bin/perl
# Sample file demonstrating Perl syntax highlighting features.
# Exercises all captures from the TS query override.

use strict;
use warnings;
require Carp;

package MyModule;

# Variable declarations (my/our/local -> magenta)
my $scalar = 42;
our @array = (1, 2, 3);
local %hash = (key => "value");

# Subroutine definition (sub -> yellow)
sub greet {
    my ($name) = @_;
    return "Hello, $name!";
}

# Control flow keywords -> magenta
if ($scalar == 42) {
    print "Found it\n";
} elsif ($scalar > 42) {
    print "Too high\n";
} else {
    print "Too low\n";
}

unless ($scalar < 0) {
    print "Positive\n";
}

# Loops
while ($scalar > 0) {
    last;
}

until ($scalar == 0) {
    next;
}

for my $i (0 .. 10) {
    redo if $i == 5;
}

foreach my $item (@array) {
    print "$item\n";
}

do {
    $scalar--;
} while ($scalar > 0);

# Logical operators
my $result = 1 and 0;
my $other = 0 or 1;

# Goto
goto DONE if $scalar == 0;

# BEGIN/END blocks
BEGIN { print "Starting\n"; }
END { print "Ending\n"; }

# Builtin functions -> yellow
chomp $scalar;
chop $scalar;
my $is_defined = defined $scalar;
undef $scalar;
eval { die "error"; };

# Operators -> yellow
my $sum = 1 + 2;
my $diff = 5 - 3;
my $prod = 4 * 2;
my $quot = 10 / 2;
my $mod = 7 % 3;
my $pow = 2 ** 8;
my $concat = "a" . "b";
my $range = 1 .. 10;

# Comparison operators
my $eq_num = (1 == 2);
my $ne_num = (1 != 2);
my $lt_num = (1 < 2);
my $gt_num = (1 > 2);
my $le_num = (1 <= 2);
my $ge_num = (1 >= 2);
my $cmp_num = (1 <=> 2);

# String comparison operators
my $eq_str = ("a" eq "b");
my $ne_str = ("a" ne "b");
my $lt_str = ("a" lt "b");
my $gt_str = ("a" gt "b");
my $le_str = ("a" le "b");
my $ge_str = ("a" ge "b");
my $cmp_str = ("a" cmp "b");

# Logical operators
my $and_op = 1 && 0;
my $or_op = 1 || 0;
my $not_op = !1;

# Assignment and regex match
my $match = ("hello" =~ /hel/);
my $nomatch = ("hello" !~ /xyz/);

# Arrow operator
my $ref = \@array;
my $val = $ref->[0];

# String literals
my $double = "Hello, world!\n";
my $single = 'Hello, world!';
my $heredoc = <<EOF;
This is a heredoc
with multiple lines
EOF

# Command string
my $cmd = `ls -la`;

# Regular expressions
my $regex = qr/pattern/;
if ("text" =~ /match/) {
    print "matched\n";
}

# Variables -> brightgreen
my @list = (10, 20, 30);
my %table = (a => 1, b => 2);
my $ref2 = \$scalar;
print $list[0];
print $table{a};

# Comment
# This is a line comment

# Semicolons -> brightmagenta
# Commas -> brightcyan
my @items = ("one", "two", "three");

# Brackets/parens -> brightcyan
my @nested = ([1, 2], {a => 1});

# Labels
DONE:
print "Reached label\n";

# Data section -> brown
__END__
This is the data section.
It should be colored as a comment/brown.
