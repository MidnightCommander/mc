#
# This makefile is for making the distribution on Unix
# You won't be able to compile with this one the Midnight Commander
# on Windows NT or OS/2
#
# Please read the file README in this directory if you want to compile
# the Midnight Commander for Windows NT or OS/2
#

include ../Make.common

FILES = BUGS Makefile Makefile.BC2 Makefile.BC5 Makefile.EMX \
	Makefile.MIN Makefile.IBM Makefile.PC Makefile.VC4 \
	Makefile.RSX README TODO ChangeLog chmod.c config.h \
	cons_nt.c cons_os2.c dirent.h dirent_nt.c dirent_os2.c \
	drive.c drive.h key_nt.c key_os2.c mc.def mc.rc mc_nt.ico \
	mc_os2.ico slint_pc.c trace_nt.c trace_nt.h util_nt.c \
	util_os2.c util_win32.c util_win32.h util_winnt.c

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
	$(CP) $(FILES) ../../mc-$(VERSION)/pc
	-mkdir ../../mc-$(VERSION)/pc/sys
	$(CP) $(FILESSYS) ../../mc-$(VERSION)/pc/sys
