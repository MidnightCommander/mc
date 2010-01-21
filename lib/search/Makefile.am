noinst_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES = \
	search.c \
	search.h \
	internal.h \
	lib.c \
	normal.c \
	regex.c \
	glob.c \
	hex.c

libsearch_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" $(PCRE_CFLAGS)
