mandir = @mandir@/sr
if USE_MCFS
man_MANS = mc.1 mcserv.8
else
man_MANS = mc.1
endif

pkgdata_DATA = mc.hlp.sr

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.sr: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp mc.1 $(srcdir)/xnc.hlp mc.hlp.sr
