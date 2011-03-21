
noinst_LTLIBRARIES = libmcevent.la

libmcevent_la_SOURCES = \
	event.c \
	internal.h \
	manage.c \
	raise.c

libmcevent_la_CFLAGS=-I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
