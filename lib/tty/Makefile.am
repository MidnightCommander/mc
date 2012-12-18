
noinst_LTLIBRARIES = libmctty.la

if USE_SCREEN_SLANG
    TTY_SCREEN_SRC = \
	color-slang.c color-slang.h \
	tty-slang.c tty-slang.h
else
    TTY_SCREEN_SRC = \
	color-ncurses.c \
	tty-ncurses.c tty-ncurses.h
endif

TTY_SRC = \
	color-internal.c color-internal.h \
	color.c color.h \
	key.c key.h keyxdef.c \
	mouse.c mouse.h \
	tty.c tty.h tty-internal.h \
	win.c win.h

if HAVE_TEXTMODE_X11_SUPPORT
TTY_SRC += x11conn.c x11conn.h
endif

libmctty_la_SOURCES = $(TTY_SRC) $(TTY_SCREEN_SRC)

AM_CPPFLAGS = -I$(top_srcdir)

if HAVE_GMODULE
AM_CPPFLAGS += $(GMODULE_CFLAGS)
else
AM_CPPFLAGS += $(GLIB_CFLAGS)
endif

