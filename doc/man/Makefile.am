DIST_SUBDIRS = es hu it pl ru sr
if USE_NLS
SUBDIRS = $(DOC_LINGUAS)
endif

man_MANS = mc.1 mcedit.1 mcview.1

CLEANFILES = $(man_MANS)

EXTRA_DIST = date-of-man-include.am

DATE_LANG=en_US.UTF-8
DATE_FORMAT=%B %Y

include date-of-man-include.am
