
noinst_LTLIBRARIES = libmcevent.la

libmcevent_la_SOURCES = \
	event.c \
	internal.h \
	manage.c \
	raise.c

AM_CPPFLAGS = \
	-DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\"

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CFLAGS)
