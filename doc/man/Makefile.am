if USE_NLS
SUBDIRS = l10n po
endif

DIST_SUBDIRS = l10n po

man_MANS = mc.1 mcedit.1 mcview.1

CLEANFILES = $(man_MANS)

EXTRA_DIST = date-of-man-include.am

DATE_LANG=en_US.UTF-8
DATE_FORMAT=%B %Y

include date-of-man-include.am
