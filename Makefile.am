## Process this file with automake to create Makefile.in.

SUBDIRS = intl po vfs slang edit src lib doc syntax pc

ACLOCAL_AMFLAGS = -I macros

ACLOCAL_M4_SOURCES = \
	macros/gnome.m4 \
	macros/gnome-vfs.m4 \
	macros/gnome-undelfs.m4 \
	macros/linger.m4

EXTRA_DIST =	FAQ INSTALL.FAST MAINTAINERS \
		README.PC README.QNX README.smbfs \
		extraconf.h \
		$(ACLOCAL_M4_SOURCES)

noinst_DATA = mc.spec

dist-hook:
	cp mc.spec $(distdir)
