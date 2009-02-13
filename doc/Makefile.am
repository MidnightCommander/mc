DIST_SUBDIRS = hints man es hu it pl ru sr
SUBDIRS = hints man $(DOC_LINGUAS)

pkgdata_DATA = mc.hlp

EXTRA_DIST = xnc.hlp $(DX_CONFIG) doc/devel doxygen-include.am
CLEANFILES = $(pkgdata_DATA)

mc.hlp: man/mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp man/mc.1 $(srcdir)/xnc.hlp mc.hlp

include doxygen-include.am
