mandir = @mandir@/ru
mclibdir = $(libdir)/mc

mclib_DATA = mc.hlp.ru

man_MANS = mc.1

EXTRA_DIST = xnc.hlp mc.hlp.ru
MAINTAINERCLEANFILES = mc.hlp.ru

mc.hlp.ru: mc.1 xnc.hlp $(top_srcdir)/src/gindex.pl $(top_srcdir)/src/man2hlp
	- perl $(top_srcdir)/src/gindex.pl mc.1 xnc.hlp mc.hlp.ru '' \
	    $(top_srcdir)/src/man2hlp

