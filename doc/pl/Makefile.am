mandir = @mandir@/pl
man_MANS = mc.1

pkgdata_DATA = mc.hlp.pl

CLEANFILES = $(pkgdata_DATA)

mc.hlp.pl: mc.1 $(top_srcdir)/doc/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(top_srcdir)/doc/xnc.hlp \
	mc.hlp.pl
