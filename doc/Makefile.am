SUBDIRS = es hu it ru

if MCSERV_INSTALL
man_MANS = mc.1 mcedit.1 mcserv.8
else
man_MANS = mc.1 mcedit.1
endif

mclibdir = $(libdir)/mc
mclib_DATA = mc.hlp

EXTRA_DIST = DEVEL xnc.hlp
CLEANFILES = $(mclib_DATA)

mc.hlp: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(srcdir)/xnc.hlp mc.hlp
