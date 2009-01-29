AM_CFLAGS = $(GLIB_CFLAGS)

localedir = $(datadir)/locale
pkglibexecdir =		$(libexecdir)/@PACKAGE@

bin_PROGRAMS = mc mcmfmt

if CONS_SAVER
pkglibexec_PROGRAMS = cons.saver
AM_CPPFLAGS = -DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" \
	-DSAVERDIR=\""$(pkglibexecdir)"\"
else
AM_CPPFLAGS = -DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"
endif

noinst_PROGRAMS = man2hlp
man2hlp_LDADD = $(GLIB_LIBS)

mcmfmt_SOURCES = mfmt.c

cons_saver_SOURCES = cons.saver.c

man2hlp_SOURCES = man2hlp.c glibcompat.c

if INCLUDED_SLANG
SLANGLIB = ../slang/libmcslang.a
endif

if USE_EDIT
EDITLIB = ../edit/libedit.a
endif

if USE_VFS
if USE_SAMBA_FS
VFSLIB = ../vfs/libvfs-mc.a ../vfs/samba/libsamba.a
else
VFSLIB = ../vfs/libvfs-mc.a
endif
endif

mc_LDADD = $(EDITLIB) $(SLANGLIB) $(VFSLIB) \
	$(INTLLIBS) $(GLIB_LIBS) $(MCLIBS) $(LIBICONV)

CHARSET_SRC = charsets.c charsets.h selcodepage.c selcodepage.h

SRCS =	achown.c achown.h background.c background.h boxes.c boxes.h	\
	chmod.c chmod.h chown.c chown.h cmd.c cmd.h color.c color.h	\
	command.c command.h complete.c cons.handler.c	\
	cons.saver.h dialog.c dialog.h dir.c dir.h			\
	eregex.h execute.c execute.h ext.c ext.h file.c filegui.c	\
	filegui.h file.h filenot.c fileopctx.c fileopctx.h find.c	\
	find.h findme.c	findme.h fs.h					\
	glibcompat.c glibcompat.h global.h help.c help.h hotlist.c	\
	hotlist.h info.c info.h key.c key.h keyxdef.c layout.c		\
	layout.h learn.c learn.h listmode.c listmode.h			\
	logging.h logging.c						\
	main.c main.h							\
	menu.c menu.h mountlist.c mountlist.h mouse.c mouse.h myslang.h	\
	option.c option.h panel.h panelize.c panelize.h poptalloca.h	\
	popt.c poptconfig.c popt.h popthelp.c poptint.h poptparse.c	\
	profile.c profile.h regex.c rxvt.c screen.c setup.c setup.h	\
	slint.c	subshell.c subshell.h textconf.c textconf.h		\
	tree.c tree.h treestore.c treestore.h timefmt.h tty.c tty.h user.c	\
	user.h util.c util.h utilunix.c view.c view.h vfsdummy.h widget.c	\
	widget.h win.c win.h wtools.c wtools.h unixcompat.h		\
	x11conn.h x11conn.c ecs.h ecs.c

if CHARSET
mc_SOURCES = $(SRCS) $(CHARSET_SRC)
else
mc_SOURCES = $(SRCS)
endif

EXTRA_DIST = ChangeLog OChangeLog man2hlp.c $(CHARSET_SRC)

# automated testing

TESTS =			ecs-test

check_PROGRAMS =	ecs-test
ecs_test_SOURCES =	ecs-test.c ecs.h ecs.c
ecs_test_LDADD =	$(GLIB_LIBS) $(INTLLIBS) $(MCLIBS)

# end of automated testing

install-exec-hook:
	$(MAKE) install_mcview
if USE_EDIT
	$(MAKE) install_mcedit
endif

#
# Make relative symlinks, but do the right thing if LN_S is `ln' or `cp'.
#
install_mcview:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcview && $(LN_S) mc mcview

install_mcedit:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcedit && $(LN_S) mc mcedit

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcview
if USE_EDIT
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcedit
endif
