# Generated automatically from Makefile.in by configure.
srcdir = .

rootdir = $(srcdir)/..
include ../Make.common

CFLAGS = $(XCFLAGS)
CPPFLAGS = $(XCPPFLAGS)
LDFLAGS = $(XLDFLAGS) 
DEFS = $(XDEFS)
LIBS =  $(XLIBS)  -lpam -ldl $(XLIB)
INSTALL = /usr/bin/ginstall -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
AR = /usr/bin/ar

#
# Distribution variables
#

EDITSRC = edit.c editcmd.c editdraw.c \
	editwidget.c gtkedit.c gtkeditkey.c \
	mousemark.c propfont.c syntax.c wordproc.c \
	config.h edit.h editcmddef.h global.h gtkedit.h \
	libgettext.h lkeysym.h mousemark.h my_string.h

EDITOBJS = edit.o editcmd.o editdraw.o \
	editwidget.o gtkedit.o gtkeditkey.o \
	mousemark.o propfont.o syntax.o wordproc.o \

DIST = 	Makefile.in README.edit $(EDITSRC)

all: libgtkedit.a

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) $(CFLAGS) -DGTK $<

check:
	@echo no tests are supplied.

libgtkedit.a: $(EDITOBJS)
	$(RMF) $@
	$(AR) cr $@ $(EDITOBJS)
	-$(RANLIB) $@

mcedit:
	-$(RMF) $(DESTDIR)$(bindir)/$(binprefix)mcedit
	$(LN_S) mc $(DESTDIR)$(bindir)/$(binprefix)mcedit

showlibdep:
	@echo 'OBJS="$(EDITOBJS)"'

cross:
	$(MAKE) CC=gcc-linux CPP="gcc-linux -E" \
	CPPFLAGS="$(CPPFLAGS) -I/usr/local/lib/gcc-lib/i386-linux-linux/include/ncurses "

TAGS: $(EDITSRC)
	etags $(EDITSRC)

clean:
	$(RMF) *.o core a.out libgtkedit.a

realclean: clean
	$(RMF) .depend
	$(RMF) TAGS
	$(RMF) *~

distclean:
	-$(RMF) $(srcdir)/*~ $(srcdir)/*.o $(srcdir)/a.out
	-$(RMF) $(srcdir)/core $(srcdir)/libgtkedit.a
	-if test $(srcdir) = .; then $(MAKE) realclean; fi
	-$(RMF) $(srcdir)/Makefile

install: mcedit

uninstall:
	-$(RMF) $(DESTDIR)$(bindir)/$(binprefix)mcedit

distcopy:
	$(CP) $(DIST) ../../mc-$(VERSION)/edit

depend dep: mcdep

fastdeploc:

# ***Dependencies***Do not edit***
ifeq (.depend,$(wildcard .depend)) 
include .depend 
endif
# ***End of dependencies***
