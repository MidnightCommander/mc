MANLANG=es
MAN2HLP=$(top_builddir)/src/man2hlp
pkgdata_DATA = mc.hlp.$(MANLANG)

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.$(MANLANG): $(top_builddir)/doc/man/$(MANLANG)/mc.1 $(srcdir)/xnc.hlp $(MAN2HLP)
	- $(MAN2HLP) $(top_builddir)/doc/man/$(MANLANG)/mc.1 $(srcdir)/xnc.hlp mc.hlp.$(MANLANG)
