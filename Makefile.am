## Process this file with automake to create Makefile.in.

AUTOMAKE_OPTIONS = 1.5

SUBDIRS = intl po m4 vfs slang edit src lib doc syntax

EXTRA_DIST = FAQ HACKING INSTALL.FAST MAINTAINERS README.QNX TODO

noinst_DATA = mc.spec
dist_noinst_SCRIPTS = build-glib1.sh build-glib2.sh

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	cp mc.spec $(distdir)
