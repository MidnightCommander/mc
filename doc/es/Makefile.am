mandir = @mandir@/es
mclibdir = $(libdir)/mc

mclib_DATA = mc.hlp.es

man_MANS = mc.1

EXTRA_DIST = xnc.hlp mc.hlp.es
MAINTAINERCLEANFILES = mc.hlp.es

$(srcdir)/mc.hlp.es: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(srcdir)/xnc.hlp \
	    $(srcdir)/mc.hlp.es
