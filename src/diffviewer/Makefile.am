noinst_LTLIBRARIES = libdiffviewer.la

libdiffviewer_la_SOURCES = \
	events.c \
	execute.c execute.h \
	internal.h \
	options.c options.h \
	search.c \
	tools.c \
	ydiff.c ydiff.h

if CHARSET
libdiffviewer_la_SOURCES += charset.c charset.h
endif


AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CPPFLAGS)

libdiffviewer_la_LIBADD = ../../lib/libmc.la
