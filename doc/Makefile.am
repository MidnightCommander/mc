SUBDIRS = ru es

if MCSERV_INSTALL
man_MANS = mc.1 mcedit.1 mcserv.8
else
man_MANS = mc.1 mcedit.1
endif

EXTRA_DIST = DEVEL xnc.hlp mc.hlp
MAINTAINERCLEANFILES = mc.hlp

mclibdir = $(libdir)/mc
mclib_DATA = mc.hlp

$(srcdir)/mc.hlp: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp 58 mc.1 $(srcdir)/xnc.hlp \
	    $(srcdir)/mc.hlp
