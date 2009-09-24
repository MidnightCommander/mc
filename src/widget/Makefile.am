
noinst_LTLIBRARIES = libmcwidget.la

libmcwidget_la_SOURCES = \
	container.c \
	container.h \
	events.c \
	events.h \
	internal.h \
	main.c \
	main.h \
	object.c \
	object.h \
	widget.c \
	widget.h \
	widget-desktop.c \
	widget-desktop.h

libmcwidget_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

