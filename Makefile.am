## Process this file with automake to create Makefile.in.
AUTOMAKE_OPTIONS = 1.14

SUBDIRS = po lib src doc contrib misc

if HAVE_TESTS
    SUBDIRS += tests
endif

EXTRA_DIST = 

dist_noinst_SCRIPTS = \
    build-glib2.sh \
    version.sh

dist_noinst_HEADERS = $(top_srcdir)/mc-version.h

ACLOCAL_AMFLAGS = -I m4

CONFIG_STATUS_DEPENDENCIES = $(top_srcdir)/mc-version.h

.PHONY: update-version

update-version:
	@if test -x $(top_srcdir)/version.sh; then \
	    $(top_srcdir)/version.sh "$(top_srcdir)" 2>&1 >/dev/null; \
	else \
	    if test ! -e $(top_srcdir)/mc-version.h; then \
	        echo "File not found: $(top_srcdir)/version.sh"; \
	        exit 1; \
	    fi; \
	fi

$(top_srcdir)/mc-version.h: update-version

INDENT_CMD = clang-format -i

INDENT_DIRS = \
    "$(top_srcdir)/lib" \
    "$(top_srcdir)/src" \
    "$(top_srcdir)/tests"

indent:
	for directory in $(INDENT_DIRS); do \
	    find "$${directory}" -name '*.[ch]' -print0 | \
	        xargs -0 $(INDENT_CMD); \
	done
