mandir = @mandir@/es
man_MANS = mc.1

pkgdata_DATA = mc.hlp.es

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.es: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp mc.1 $(srcdir)/xnc.hlp mc.hlp.es
