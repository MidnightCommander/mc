## Process this file with automake to create Makefile.in.

SUBDIRS = vfs lib doc slang gtkedit edit src gnome new_icons \
	idl syntax intl doc-gnome po pc

ACLOCAL_AMFLAGS = -I macros

ACLOCAL_M4_SOURCES = \
	macros/gnome.m4 \
	macros/gnome-vfs.m4 \
	macros/gnome-undelfs.m4 \
	macros/gnome-gettext.m4 \
	macros/linger.m4

EXTRA_DIST =	BUGS FAQ INSTALL.FAST MAINTAINERS \
		README.PC README.QNX README.smbfs \
		$(ACLOCAL_M4_SOURCES)

noinst_DATA = mc.spec

dist-hook:
	cp mc.spec $(distdir)
