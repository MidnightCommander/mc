noinst_LTLIBRARIES = libmcskin.la

libmcskin_la_SOURCES = \
	colors.c \
	colors-old.c \
	common.c \
	hc-skins.c \
	ini-file.c \
	lines.c \
	skin.h \
	internal.h

libmcskin_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

libmcskin_la_LIBADD = $(PCRE_LIBS)
