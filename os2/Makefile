#
# This makefile is for making the distribution on Unix
# You won't be able to compile with this one the Midnight Commander
# on OS/2, use Makefile.release instead.
#
# Please read the file README.OS2 in the top directory of the distribution
#

include ../Make.common

FILES =	coollocal.h gpm-xterm.h lkeysym.h pwd.h Makefile coolwidget.h grp.h \
	makefile.debug slint.os2.c chmod.os2.c dirent.h inst.cmd \
	makefile.release config.h dirent.os2.c mc.def termios.h \
	cons.handler.os2.c drive.h key.os2.c mc.ico unistd.h cons.saver.h \
	drive.os2.c learn.h util.os2.c

FILESSYS = sys/param.h sys/time.h sys/wait.h

FILESEDIT = os2edit/copyright.h os2edit/dirtools.h \
	os2edit/makefile.debug os2edit/makefile.release os2edit/makefile.rf
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
	-mkdir ../../mc-$(VERSION)/os2/os2edit
	$(CP) $(FILESEDIT) ../../mc-$(VERSION)/os2/os2edit
