## Process this file with automake to create Makefile.in.

AUTOMAKE_OPTIONS = 1.5

SUBDIRS = intl po m4 vfs slang edit src lib doc syntax

EXTRA_DIST = FAQ INSTALL.FAST MAINTAINERS README.QNX

noinst_DATA = mc.spec

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	cp mc.spec $(distdir)
