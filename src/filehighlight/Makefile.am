noinst_LTLIBRARIES = libmcfilehighlight.la

libmcfilehighlight_la_SOURCES = \
	common.c \
	fhl.h \
	get-color.c \
	ini-file-read.c \
	internal.h

libmcfilehighlight_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" 

libmcfilehighlight_la_LIBADD = $(PCRE_LIBS)
