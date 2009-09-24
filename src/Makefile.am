SUBDIRS = mcconfig search tty viewer filehighlight skin widget

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

localedir = $(datadir)/locale
pkglibexecdir =		$(libexecdir)/@PACKAGE@

bin_PROGRAMS = mc mcmfmt

if CONS_SAVER
pkglibexec_PROGRAMS = cons.saver
AM_CPPFLAGS = -DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" \
	-DSAVERDIR=\""$(pkglibexecdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\"
else
AM_CPPFLAGS = -DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\" \
	-DPACKAGE=\""@PACKAGE@\"
endif

noinst_PROGRAMS = man2hlp
man2hlp_LDADD = $(GLIB_LIBS)

mcmfmt_SOURCES = mfmt.c

cons_saver_SOURCES = cons.saver.c

man2hlp_SOURCES = man2hlp.c glibcompat.c

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

MCCONFIGLIB = mcconfig/libmcconfig.la
SEARCHLIB = search/libsearch.la
TTYLIB =  tty/libmctty.la
VIEWERLIB = viewer/libmcviewer.la
FILEHIGHLIGHTLIB=filehighlight/libmcfilehighlight.la
SKINLIB =  skin/libmcskin.la
WIDGETLIB =  widget/libmcwidget.la

mc_LDADD = $(EDITLIB) $(VFSLIB) $(FILEHIGHLIGHTLIB) $(SKINLIB) \
	$(MCCONFIGLIB) $(SEARCHLIB) $(TTYLIB) $(VIEWERLIB) $(WIDGETLIB) \
	$(INTLLIBS) $(MCLIBS) $(SLANGLIB) $(LIBICONV) $(GLIB_LIBS)

CHARSET_SRC = charsets.c charsets.h selcodepage.c selcodepage.h

SRCS =	args.c args.h achown.c achown.h background.c background.h	\
	boxes.c boxes.h chmod.c chmod.h chown.c chown.h cmd.c cmd.h	\
	command.c command.h complete.c cons.handler.c			\
	cons.saver.h dialog.c dialog.h dir.c dir.h			\
	execute.c execute.h ext.c ext.h file.c filegui.c		\
	filegui.h file.h filenot.c fileopctx.c fileopctx.h find.c	\
	find.h fs.h							\
	glibcompat.c glibcompat.h global.h help.c help.h hotlist.c	\
	hotlist.h info.c info.h layout.c				\
	layout.h learn.c learn.h listmode.c listmode.h history.h	\
	logging.h logging.c main.c main.h main-widgets.h		\
	menu.c menu.h mountlist.c mountlist.h				\
	option.c option.h panel.h panelize.c panelize.h screen.c	\
	setup.c setup.h  subshell.c subshell.h textconf.c textconf.h	\
	tree.c tree.h treestore.c treestore.h timefmt.h user.c		\
	user.h util.c util.h utilunix.c vfsdummy.h 			\
	widget.c widget.h wtools.c wtools.h unixcompat.h		\
	ecs.h ecs.c							\
	keybind.c keybind.h cmddef.h					\
	strutil.h strutil.c strutilascii.c strutil8bit.c strutilutf8.c	\
	search/search.h strescape.c strescape.h

if CHARSET
mc_SOURCES = $(SRCS) $(CHARSET_SRC)
else
mc_SOURCES = $(SRCS)
endif

EXTRA_DIST = man2hlp.c $(CHARSET_SRC)

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
