mandir = @mandir@/es
man_MANS = mc.1

mclibdir = $(libdir)/mc
mclib_DATA = mc.hlp.es

EXTRA_DIST = xnc.hlp
CLEANFILES = $(mclib_DATA)

mc.hlp.es: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(srcdir)/xnc.hlp mc.hlp.es
