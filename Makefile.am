## Process this file with automake to create Makefile.in.

SUBDIRS = vfs lib doc slang gtkedit edit src gnome new_icons \
	icons idl syntax intl doc-gnome po pc

ACLOCAL_AMFLAGS = -I macros

GNOME_M4_FILES = macros/gnome.m4 macros/gnome-vfs.m4 macros/gnome-undelfs.m4 \
	macros/linger.m4

EXTRA_DIST =	BUGS FAQ INSTALL.FAST MAINTAINERS \
		README.PC README.QNX README.smbfs \
		create_vcs mc-aclocal.m4 gettext.m4 \
		$(GNOME_M4_FILES)

dist-hook:
	cp mc.spec $(distdir)
