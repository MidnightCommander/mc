noinst_LTLIBRARIES = libmctty.la

libmctty_la_SOURCES = \
	color.c color.h \
	key.c key.h keyxdef.c \
	mouse.c mouse.h \
	myslang.h slint.c \
	tty.c tty.h

libmctty_la_CFLAGS = -I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
