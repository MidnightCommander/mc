AM_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@

if INCLUDED_SLANG
lib_LIBRARIES = libmcslang.a
else
lib_LIBRARIES =
endif

libmcslang_a_SOURCES = sldisply.c slerr.c slsmg.c slutty.c slgetkey.c \
	slmemcpy.c slmemset.c sltermin.c sltoken.c slsignal.c \
	slang-mc.h _slang.h sl-feat.h jdmacros.h

EXTRASRC = slvideo.c slw32tty.c slos2tty.c
EXTRADOCS = ChangeLog README

EXTRA_DIST = $(EXTRASRC) $(EXTRADOCS)
