
noinst_LTLIBRARIES = libmcconfig.la

libmcconfig_la_SOURCES = \
	common.c \
	get.c \
	set.c \
	paths.c

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\"

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS)
