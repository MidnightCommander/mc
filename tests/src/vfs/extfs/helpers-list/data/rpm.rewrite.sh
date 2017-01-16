#
# This file gets source'd into our rpm helper.
#
# It imitates the 'rpm' program by overriding a few functions.
#

# The tags file.
TAGSF="$MC_TEST_EXTFS_INPUT"

. "$MC_TEST_EXTFS_CONFIG_SH"  # Gain access to $PERL.

# ----------------------------------------------------------------------------

#
# Overrides helper's.
#
# Imitates 'rpm --querytags'.
#
# @Mock
#
mcrpmfs_getSupportedTags()
{
  $PERL -e '
    $tagsf = $ARGV[0];

    do $tagsf or die("$tagsf: $!");
    print join("\n", keys %$tags);
  ' \
  "$TAGSF"
}

# ----------------------------------------------------------------------------

#
# Imitates 'rpm -qp --qf <TEMPLATE> <PACKAGE_FILE>'.
#
# (It ignores <PACKAGE_FILE>, using our input instead.)
#
# E.g.: given "Name: %{NAME} Ver: %{VERSION}",
# prints "Name: php-pear-Twig Ver: 1.0.0".
#
rpm_qf()
{
  $PERL -w -e '
    $tagsf = $ARGV[0];
    $tmplt = $ARGV[1];

    do $tagsf or die("$tagsf: $!");
    $tmplt =~ s/\\n/\n/g;
    $tmplt =~ s/%\{(.*?)\}/
      (my $tag = $1) =~ s,^RPMTAG_,,;  # Tag names may be specified with or without this prefix.
      exists $tags->{$tag} ? $tags->{$tag} : "(none)"
    /eg;
    print $tmplt;
  ' \
  "$TAGSF" "$1"
}
RPM_QUERY_FMT=rpm_qf  # Tell the helper to use it instead of the 'rpm' binary.

# ----------------------------------------------------------------------------

#
# Overrides helper's.
#
# @Mock
#
mcrpmfs_getDesription()
{
  rpm_qf "%{_INFO}"
}

# ----------------------------------------------------------------------------
