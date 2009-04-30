noinst_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES = \
	search.c \
	search.h \
	internal.h \
	lib.c \
	normal.c \
	regex.c \
	glob.c

libsearch_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
