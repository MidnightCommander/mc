mandir = @mandir@/ru
mclibdir = $(libdir)/mc

mclib_DATA = mc.hlp.ru

man_MANS = mc.1

EXTRA_DIST = xnc.hlp mc.hlp.ru
MAINTAINERCLEANFILES = mc.hlp.ru

$(srcdir)/mc.hlp.ru: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(srcdir)/xnc.hlp \
	    $(srcdir)/mc.hlp.ru
