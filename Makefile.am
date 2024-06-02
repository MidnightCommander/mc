## Process this file with automake to create Makefile.in.
AUTOMAKE_OPTIONS = 1.12

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

.PHONY: update-version \
        cppcheck \
        cppcheck-error \
        cppcheck-information \
        cppcheck-performance \
        cppcheck-portability \
        cppcheck-style \
        cppcheck-warning \
        cppcheck-all


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

CPPCHECK_CMD = cppcheck \
    --inline-suppr \
    --error-exitcode=0 \
    -j 4 \
    --force \
    -I $(top_srcdir)/lib \
    --language=c \
    --std=c99

CPPCHECK_DIRS = \
    "$(top_srcdir)/lib" \
    "$(top_srcdir)/src"

CPPCHECK_OUT_PREFIX = $(top_builddir)/cppcheck-

CPPCHECK_OUT_EXT = log.txt

CPPCHECK_SED_FILTER = \
    -e '/is reassigned a value before the old one has been used./d' \
    -e '/ Unmatched suppression: /d' \
    -e "/Skipping configuration '.*' since the value of '.*' is unknown./d"

cppcheck:
	$(CPPCHECK_CMD) --enable=all $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)all.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)all.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)all.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)all.$(CPPCHECK_OUT_EXT).tmp

cppcheck-all: cppcheck

cppcheck-information:
	$(CPPCHECK_CMD) --enable=information $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)information.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)information.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)information.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)all.$(CPPCHECK_OUT_EXT).tmp

cppcheck-performance:
	$(CPPCHECK_CMD) --enable=performance $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)performance.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)performance.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)performance.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)performance.$(CPPCHECK_OUT_EXT).tmp

cppcheck-portability:
	$(CPPCHECK_CMD) --enable=portability $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)portability.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)portability.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)portability.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)portability.$(CPPCHECK_OUT_EXT).tmp

cppcheck-style:
	$(CPPCHECK_CMD) --enable=style $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)style.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)style.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)style.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)style.$(CPPCHECK_OUT_EXT).tmp

cppcheck-warning:
	$(CPPCHECK_CMD) --enable=warning $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)warning.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)warning.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)warning.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)warning.$(CPPCHECK_OUT_EXT).tmp

cppcheck-unusedFunction:
	$(CPPCHECK_CMD) --enable=unusedFunction $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)unusedFunction.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)unusedFunction.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)unusedFunction.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)unusedFunction.$(CPPCHECK_OUT_EXT).tmp

cppcheck-missingInclude:
	$(CPPCHECK_CMD) --enable=missingInclude $(CPPCHECK_DIRS) 2>$(CPPCHECK_OUT_PREFIX)missingInclude.$(CPPCHECK_OUT_EXT).tmp;
	$(SED) $(CPPCHECK_SED_FILTER) $(CPPCHECK_OUT_PREFIX)missingInclude.$(CPPCHECK_OUT_EXT).tmp > $(CPPCHECK_OUT_PREFIX)missingInclude.$(CPPCHECK_OUT_EXT)
	rm -f $(CPPCHECK_OUT_PREFIX)missingInclude.$(CPPCHECK_OUT_EXT).tmp

cppcheck-split-all: \
    cppcheck-information \
    cppcheck-performance \
    cppcheck-portability \
    cppcheck-style \
    cppcheck-warning \
    cppcheck-unusedFunction \
    cppcheck-missingInclude

INDENT_CMD = $(INDENT) \
    --gnu-style \
    --format-first-column-comments \
    --indent-level4 \
    --brace-indent0 \
    --line-length100 \
    --no-tabs \
    --blank-lines-after-procedures

INDENT_DIRS = \
    "$(top_srcdir)/lib" \
    "$(top_srcdir)/src" \
    "$(top_srcdir)/tests"

indent:
	for directory in $(INDENT_DIRS); do \
	    find "$${directory}" -name '*.[ch]' -print0 | \
	        xargs -0 $(INDENT_CMD); \
	done
