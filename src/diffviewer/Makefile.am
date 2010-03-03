noinst_LTLIBRARIES = libdiffviewer.la

libdiffviewer_la_SOURCES = \
	ydiff.c \
	ydiff.h

libdiffviewer_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
