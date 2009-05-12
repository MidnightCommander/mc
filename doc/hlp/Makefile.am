DIST_SUBDIRS = es hu it pl ru sr
SUBDIRS = $(DOC_LINGUAS)

pkgdata_DATA = mc.hlp

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp: $(top_builddir)/doc/man/mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp $(top_builddir)/doc/man/mc.1 $(srcdir)/xnc.hlp mc.hlp
