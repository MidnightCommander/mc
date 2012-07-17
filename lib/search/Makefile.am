noinst_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES = \
	search.c \
	internal.h \
	lib.c \
	normal.c \
	regex.c \
	glob.c \
	hex.c

AM_CPPFLAGS = \
	-DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\"

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CFLAGS)
