
noinst_LTLIBRARIES = libmcconfig.la

libmcconfig_la_SOURCES = \
	common.c \
	get.c \
	set.c

libmcconfig_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

