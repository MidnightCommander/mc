mandir = @mandir@/it
man_MANS = mc.1

pkgdata_DATA = mc.hlp.it

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.it: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp mc.1 $(srcdir)/xnc.hlp mc.hlp.it
