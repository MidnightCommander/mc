LANG=hu
MAN2HLP=$(top_builddir)/src/man2hlp/man2hlp
pkgdata_DATA = mc.hlp.$(LANG)

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.$(LANG): $(top_builddir)/doc/man/$(LANG)/mc.1 $(srcdir)/xnc.hlp $(MAN2HLP)
	- $(MAN2HLP) $(top_builddir)/doc/man/$(LANG)/mc.1 $(srcdir)/xnc.hlp mc.hlp.$(LANG)
