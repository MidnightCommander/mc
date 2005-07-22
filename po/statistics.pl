#! /usr/bin/env perl
use strict;
use warnings;

my $first = 1;

foreach my $f (@ARGV) {
	my ($lang, $translated, $fuzzy, $untranslated) = (undef, 0, 0, 0);

	# statistics are printed on stderr, as well as error messages.
	my $stat = `msgfmt --statistics "$f" 2>&1`;
	($lang = $f) =~ s,\.po$,,;
	if ($stat =~ qr"(\d+)\s+translated") {
		$translated = $1;
	}
	if ($stat =~ qr"(\d+)\s+fuzzy") {
		$fuzzy = $1;
	}
	if ($stat =~ qr"(\d+)\s+untranslated") {
		$untranslated = $1;
	}
	if ($first) {
		printf("%8s  %10s  %5s  %12s\n",
			"language", "translated", "fuzzy", "untranslated");
		printf("%s\n", "-" x 43);
		$first = 0;
	}
	printf("%8s  %10d  %5d  %12d\n",
		$lang, $translated, $fuzzy, $untranslated);
}
