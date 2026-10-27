LANG=ro
mandir = @mandir@/$(LANG)

EXTRA_DIST = \
	mc.1.in \
	mcedit.1.in \
	mcview.1.in \
	mcdiff.1.in

man_MANS = mc.1 mcview.1

if USE_INTERNAL_EDIT
man_MANS += mcedit.1
endif

if USE_DIFF
man_MANS += mcdiff.1
endif

CLEANFILES = $(man_MANS)

DATE_LANG=ro_RO.UTF-8
DATE_FORMAT=%B %Y

include ../date-of-man-include.am
