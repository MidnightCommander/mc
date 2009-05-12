LANG=it
pkgdata_DATA = mc.hlp.$(LANG)

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.$(LANG): $(top_builddir)/doc/man/$(LANG)/mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp $(top_builddir)/doc/man/$(LANG)/mc.1 $(srcdir)/xnc.hlp mc.hlp.$(LANG)
