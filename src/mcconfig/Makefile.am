
noinst_LTLIBRARIES = libmcconfig.la

libmcconfig_la_SOURCES = \
	mcconfig.h \
	common.c \
	get.c \
	set.c

libmcconfig_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

