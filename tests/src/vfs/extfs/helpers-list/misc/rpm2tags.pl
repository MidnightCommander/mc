use strict;
use warnings;
use Data::Dumper;

my $help = <<EOF;
WHAT
----

This script converts an RPM package into what we call a "tags file",
which is simply an associative array listing all the tags and their
values.

WHY
---

For our rpm helper tests we need an RPM file. But we also want the tests
to be able to run on systems where the rpm binary isn't installed. The
solution: our tests work on "tags files" instead of real RPM files.

HOW
---

Use it like this:

    \$ perl $0 /path/to/some/package.rpm > test.input

You can run this script only on systems where 'rpm' is installed.

WHEW
----

After all was done the author of this script discovered something not
mentioned in rpm's manpage: one can do "rpm -qp --xml pkg.rpm" to
serialize the metadata and get the same effect. But this would just move
the complexity to the test (in handling XML).
EOF

# Tag variations we're interested in and wish to record as well.
my @formatted_tags = qw(
  BUILDTIME:date
  CHANGELOGTIME:date

  REQUIREFLAGS:depflags
  PROVIDEFLAGS:depflags
  OBSOLETEFLAGS:depflags
  CONFLICTFLAGS:depflags
);

# Big tags we want to omit to save space.
my %skip = map {$_ => 1} qw(
  HEADERIMMUTABLE
);

# Make 'rpm' print dates in the format our helper expects.
$ENV{'LC_TIME'} = 'C';

sub rpm2tags
{
  my ($rpm_pkg) = @_;

  my %tags = ();

  chomp(my @all_tags = `rpm --querytags`);
  @all_tags = grep {!$skip{$_}} @all_tags;

  foreach my $tag (@all_tags, @formatted_tags) {
    $tags{$tag} = `rpm -qp --qf "%{$tag}" "$rpm_pkg"`;
  }

  # Add pseudo tags:
  $tags{'_INFO'} = `rpm -qip "$rpm_pkg"`;

  return %tags;
}

sub prolog
{
  my $cmdline = join(' ', 'perl', $0, @ARGV);
  return <<EOF;
# -*- mode: perl -*-
# vim: filetype=perl
#
# This "tags file" was created by running the following command:
#
#    \$ $cmdline
#
# This file is used in our tests instead of the corresponding RPM file.
# This lets us run the tests on systems where 'rpm' is not installed.
EOF
}

sub main
{
  if (!$ARGV[0] || $ARGV[0] eq "--help" || $ARGV[0] eq "-h") {
    print $help;
    exit;
  }
  my %tags = rpm2tags($ARGV[0]);
  $Data::Dumper::Terse = 1;
  print(prolog(), "\n", '$tags = ', Dumper(\%tags), ";\n");
}

main();
