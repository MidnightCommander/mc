#
# This makefile is for making the distribution on Unix
# You won't be able to compile with this one the Midnight Commander
# on OS/2, use Makefile.release instead.
#
# Please read the file README.OS2 in the top directory of the distribution
#

include ../Make.common

FILES = Makefile Makefile.EMX Makefile.BC2 Makefile.OS2 chmod.os2.c config.h \
	cons.handler.os2.c cons.saver.h direct.h dirent.h dirent.os2.c \
	drive.h drive.os2.c inst.cmd key.os2.c mc.def mc.ico slint.os2.c \
	util.os2.c

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
	-mkdir ../../mc-$(VERSION)/os2
	$(CP) $(FILES) ../../mc-$(VERSION)/os2
	-mkdir ../../mc-$(VERSION)/os2/sys
	$(CP) $(FILESSYS) ../../mc-$(VERSION)/os2/sys
