
noinst_LTLIBRARIES = libmcwidget.la

libmcwidget_la_SOURCES = \
	dialog.c dialog.h \
	dialog-switch.c dialog-switch.h \
	menu.c menu.h \
	widget.c widget.h \
	wtools.c wtools.h

libmcwidget_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" 

