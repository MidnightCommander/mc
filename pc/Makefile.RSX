# Makefile.RSX
#
# Midnight Commander for NT makefile
# for RSX
#
# Hacked by Dan Nicolaescu from Visual IDE mak
# Hacked by Pavel Roskin to make it work with cmd.exe from Windows NT4
# 980206 hacked by Pavel Roskin to make it work with GNU make
# --------------------------------------------------------------------------

TARGET_OS=NT

CC=gcc.exe
LINK=gcc.exe
OBJ_SUFFIX=o
OBJ_PLACE=-o
EXE_PLACE=-o
#      Just comment RSC out if you have problems with resources
RSC=grc.exe
RES_PLACE=-o

# ---- Compiler-specific optional stuff
MC_MISC_CFLAGS=-Zrsx32

ifndef RELEASE
# ---- Debug build
OBJS_DIR=debug
EXTRA_MC_SRCS=
SPECIFIC_DEFINES=
SPECIFIC_MC_CFLAGS=-g -O0 $(MC_MISC_CFLAGS)
SPECIFIC_MC_LFLAGS_EXTRA=
SPECIFIC_SLANG_CFLAGS=$(SPECIFIC_MC_CFLAGS)
SPECIFIC_MCEDIT_CFLAGS=$(SPECIFIC_MC_CFLAGS)
RC_DEFINES=
else
# ---- Release build
OBJS_DIR=release
EXTRA_MC_SRCS=
SPECIFIC_DEFINES=
SPECIFIC_MC_CFLAGS=-O2 $(MC_MISC_CFLAGS)
SPECIFIC_MC_LFLAGS_EXTRA=
SPECIFIC_SLANG_CFLAGS=$(SPECIFIC_MC_CFLAGS)
SPECIFIC_MCEDIT_CFLAGS=$(SPECIFIC_MC_CFLAGS)
RC_DEFINES=
endif

# ---- Compiler independent defines
include Makefile.PC

# ---- Linkers are very compiler-specific

SPECIFIC_MC_LFLAGS=-Zrsx32 $(SPECIFIC_MC_LFLAGS_EXTRA)
MC_LIBS=-lvideont -ladvapi32

$(MC_EXE): $(MC_RES) $(OBJS) $(MCEDIT_OBJS) $(SLANG_OBJS)
	$(LINK) $(EXE_PLACE) $(MC_EXE) $(SPECIFIC_MC_LFLAGS) $+ $(MC_LIBS)

resources: $(MC_RES)
	rsrc $(MC_EXE) $(MC_RES)
