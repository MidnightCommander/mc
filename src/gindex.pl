#!/usr/bin/perl -w
# Since we use a linear search trought the block and the license and
# the warranty are quite big, we leave them at the end of the help file,
# the index will be consulted quite frequently, so we put it at the beginning. 

@help_file = <>;

foreach $line (@help_file){
    if ($line =~ /\x04\[(.*)\]/ && $line !~ /\x04\[main\]/){
	push @nodes, $1;
	$line =~ s/(\x04\[) */$1/;
    }
}

print "\x04[Contents]\nTopics:\n\n";
foreach $node (@nodes){
    if (length $node){
	$node =~ m/^( *)(.*)$/;
	printf ("  %s\x01 %s \x02%s\x03", $1, $2, $2);
    }
    print "\n";
}
#foreach $line (@help_file){
#    $line =~ s/%NEW_NODE%/\004/g;
#}
print @help_file;
