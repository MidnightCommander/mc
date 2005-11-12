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
	include/sllimits.h \
	include/sllower.h \
	include/slupper.h \
	include/slwcwidth.h

libmcslang_a_SOURCES = \
	slcommon.c \
	sldisply.c \
	slerr.c \
	slgetkey.c \
	sllower.c \
	slmisc.c \
	slsignal.c \
	slsmg.c \
	slstring.c \
	sltermin.c \
	slupper.c \
	slutf8.c \
	slutty.c \
	slwcwidth.c \
	$(SLANG_INCLUDES)

EXTRASRC = slvideo.c slw32tty.c
EXTRADOCS = README

EXTRA_DIST = $(EXTRASRC) $(EXTRADOCS)
