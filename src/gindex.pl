#! /usr/bin/perl -w
# Since we use a linear search trought the block and the license and
# the warranty are quite big, we leave them at the end of the help file,
# the index will be consulted quite frequently, so we put it at the beginning. 

if ($#ARGV == 3) {
    $Topics = "$ARGV[3]";
} elsif ($#ARGV == 2) {
    $Topics = 'Topics:';
} else {
    die "Usage: gindex.pl man_file tmpl_file out_file [Topic section header]";
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
	if (length $1) {
	    $node = $1;
	} else {
	    push @nodes, '';
	}
	$line =~ s/(\x04\[) */$1/;
    } elsif (defined ($node)){
	if ($line ne "\n") {
	    push @nodes, "$node\x02$line";
	} else {
	    push @nodes, "$node\x02$node";
	}
	undef ($node);
    }
}

unlink ("$out_file");
if (-e "$out_file") {
    die "Cannot remove $out_file\n";
}

open (OUTPUT, "> $out_file") or die "Cannot open $out_file: $!\n";

print OUTPUT "\x04[Contents]\n$Topics\n\n";
foreach $node (@nodes){
    if (length $node){
	$node =~ m/^( *)(.*)\x02(.*)$/;
	print OUTPUT ("  $1\x01 $3 \x02$2\x03");
    }
    print OUTPUT "\n";
}

print OUTPUT @help_file;

close (OUTPUT);
