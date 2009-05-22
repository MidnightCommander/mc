DIST_SUBDIRS = hints man es hu it pl ru sr
SUBDIRS = hints man $(DOC_LINGUAS)

pkgdata_DATA = mc.hlp

DOCS = FAQ HACKING INSTALL INSTALL.FAST MAINTAINERS
EXTRA_DIST = xnc.hlp $(DX_CONFIG) doxygen-include.am $(pkgdata_DATA) $(DOCS)
CLEANFILES = $(pkgdata_DATA)

mc.hlp: man/mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp man/mc.1 $(srcdir)/xnc.hlp mc.hlp

include doxygen-include.am
