DIST_SUBDIRS = es hu it pl ru sr
SUBDIRS = $(DOC_LINGUAS)

if USE_MCFS
man_MANS = mc.1 mcedit.1 mcview.1 mcserv.8
else
man_MANS = mc.1 mcedit.1 mcview.1
endif

pkgdata_DATA = mc.hlp

EXTRA_DIST = xnc.hlp $(DX_CONFIG) doc/devel doxygen-include.am
CLEANFILES = $(pkgdata_DATA)

mc.hlp: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp mc.1 $(srcdir)/xnc.hlp mc.hlp

include doxygen-include.am
