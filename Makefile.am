## Process this file with automake to create Makefile.in.
AUTOMAKE_OPTIONS = 1.5

SUBDIRS = intl po vfs src doc syntax contrib misc

EXTRA_DIST = 

dist_noinst_SCRIPTS = build-glib2.sh

dist_noinst_HEADERS = $(top_srcdir)/version.h

ACLOCAL_AMFLAGS = -I m4

CONFIG_STATUS_DEPENDENCIES = $(top_srcdir)/version.h

.PHONY: update-version

update-version:
	@if test -x $(top_srcdir)/maint/version.sh; then \
	    $(top_srcdir)/maint/version.sh "$(top_srcdir)" 2>&1 >/dev/null; \
	else \
	    echo "File not found: $(top_srcdir)/maint/version.sh"; \
	fi

$(top_srcdir)/version.h: update-version
