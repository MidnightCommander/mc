#!/usr/bin/perl -w
# Since we use a linear search trought the block and the license and
# the warranty are quite big, we leave them at the end of the help file,
# the index will be consulted quite frequently, so we put it at the beginning. 

if ($#ARGV != 2) {
    die "Three arguments required";
}

$man_file = "$ARGV[0]";
$tmpl_file = "$ARGV[1]";
$out_file = "$ARGV[2]";

$help_width = 58;

open (HELP1, "./man2hlp $help_width $man_file |") or
    die "Cannot open read output of man2hlp: $!\n";;
@help_file = <HELP1>;
close (HELP1);

open (HELP2, "< $tmpl_file") or die "Cannot open $tmpl_file: $!\n";
push @help_file, <HELP2>;
close (HELP2);

foreach $line (@help_file){
    if ($line =~ /\x04\[(.*)\]/ && $line !~ /\x04\[main\]/){
	push @nodes, $1;
	$line =~ s/(\x04\[) */$1/;
    }
}

unlink ("$out_file");
if (-e "$out_file") {
    die "Cannot remove $out_file\n";
}

open (OUTPUT, "> $out_file") or die "Cannot open $out_file: $!\n";

print OUTPUT "\x04[Contents]\nTopics:\n\n";
foreach $node (@nodes){
    if (length $node){
	$node =~ m/^( *)(.*)$/;
	printf OUTPUT ("  %s\x01 %s \x02%s\x03", $1, $2, $2);
    }
    print OUTPUT "\n";
}

print OUTPUT @help_file;

close (OUTPUT);
