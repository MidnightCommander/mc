#
# This makefile is for making the distribution on Unix
# You won't be able to compile with this one the Midnight Commander
# on Windows NT, use Makefile.NT instead.
#
# Please read the file README.NT in the top directory of the distribution
#

include ../Make.common

FILES =	Makefile TODO dirent.c mc.ico bugs mc.rc config.h dirent.h \
	Makefile.NT chmod.nt.c drive.nt.c slint.nt.c cons.handler.nt.c \
	key.nt.c utilnt.c Makefile.NT Makefile.VC4 Makefile.BC5 \
	util.WinNT.c util.debug.c util.Win32.c util.Win32.h util.debug.h \
	drive.h

FILESSYS = sys/param.h sys/time.h

all:

TAGS: $(FILES)
	etags $(FILES)

clean realclean distclean cross:

install:

uninstall:

depend dep:

fastdeploc:

distcopy:
	$(CP) $(FILES) ../../mc-$(VERSION)/nt
	-mkdir ../../mc-$(VERSION)/nt/sys
	$(CP) $(FILESSYS) ../../mc-$(VERSION)/nt/sys
