AM_CFLAGS = $(GLIB_CFLAGS)
AM_CPPFLAGS = -I$(srcdir)/include

if INCLUDED_SLANG
noinst_LIBRARIES = libmcslang.a
else
noinst_LIBRARIES =
endif

SLANG_INCLUDES = \
	include/_slang.h \
	include/jdmacros.h \
	include/sl-feat.h \
	include/slang.h \
	include/slinclud.h \
	include/sllimits.h

libmcslang_a_SOURCES = \
	sldisply.c \
	slerr.c \
	slgetkey.c \
	slmisc.c \
	slsignal.c \
	slsmg.c \
	sltermin.c \
	slutty.c \
	$(SLANG_INCLUDES)

EXTRASRC = slvideo.c slw32tty.c
EXTRADOCS = ChangeLog README

EXTRA_DIST = $(EXTRASRC) $(EXTRADOCS)
