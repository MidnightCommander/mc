
noinst_LTLIBRARIES = libmctty.la

if USE_SCREEN_SLANG
    TTY_SCREEN_SRC = \
	color-slang.c color-slang.h \
	tty-slang.c tty-slang.h
else
    TTY_SCREEN_SRC = \
	color-ncurses.c color-ncurses.h \
	tty-ncurses.c tty-ncurses.h
endif

TTY_SRC = \
	color-internal.c color-internal.h \
	color.c color.h \
	key.c key.h keyxdef.c \
	mouse.c mouse.h \
	tty.c tty.h tty-internal.h \
	win.c win.h \
	x11conn.c x11conn.h

libmctty_la_SOURCES = $(TTY_SRC) $(TTY_SCREEN_SRC)

libmctty_la_CFLAGS = -I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
