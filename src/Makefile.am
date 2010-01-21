SUBDIRS = viewer

if USE_EDIT
SUBDIRS += editor
endif

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
	-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\"
endif

noinst_PROGRAMS = man2hlp

mcmfmt_SOURCES = mfmt.c

cons_saver_SOURCES = cons.saver.c

man2hlp_SOURCES = man2hlp.c

man2hlp_LDADD = \
    ../lib/libmc.la \
    $(GLIB_LIBS) $(PCRE_LIBS)

if USE_EDIT
EDITLIB = editor/libedit.la
endif

SRC_maintainer=logging.c logging.h
SRC_charset = charsets.c charsets.h selcodepage.c selcodepage.h

if USE_MAINTAINER_MODE
    SRC_USE_maintainer=$(SRC_maintainer)
endif

if CHARSET
    SRC_USE_charset=$(SRC_charset)
endif

mc_LDADD = \
	../lib/libmc.la \
	$(EDITLIB) \
	viewer/libmcviewer.la \
	$(INTLLIBS) $(MCLIBS) $(SLANGLIB) $(LIBICONV) \
	$(GLIB_LIBS) $(PCRE_LIBS)

if USE_SAMBA_FS
# this is a hack for linking with own samba library in simple way
mc_LDADD += ../lib/vfs/mc-vfs/samba/libsamba.a
endif


SRC_mc_widgets = \
	dialog.c dialog.h \
	menu.c menu.h \
	screen.c panel.h \
	tree.c tree.h \
	widget.c widget.h \
	wtools.c wtools.h

SRC_mc_strutils = \
	strescape.c strescape.h

SRC_mc_conssaver = \
	cons.handler.c cons.saver.h

SRC_mc_options = \
	learn.c learn.h \
	option.c option.h \
	setup.c setup.h

SRC_mc_keybind = \
	keybind.c keybind.h \
	cmddef.h

SRC_mc_extended_charset = \
	ecs.c ecs.h

mc_SOURCES = \
	$(SRC_USE_maintainer) \
	$(SRC_USE_charset) \
	$(SRC_mc_widgets) \
	$(SRC_mc_strutils) \
	$(SRC_mc_conssaver) \
	$(SRC_mc_options) \
	$(SRC_mc_keybind) \
	$(SRC_mc_extended_charset) \
	achown.c achown.h \
	args.c args.h \
	background.c background.h \
	boxes.c boxes.h \
	chmod.c chmod.h \
	chown.c chown.h \
	cmd.c cmd.h \
	command.c command.h \
	complete.c \
	dir.c dir.h \
	execute.c execute.h \
	ext.c ext.h \
	file.c file.h \
	filegui.c filegui.h \
	filenot.c \
	fileopctx.c fileopctx.h \
	find.c find.h \
	help.c help.h \
	history.h \
	hotlist.c hotlist.h \
	info.c info.h \
	layout.c layout.h \
	listmode.c listmode.h \
	main.c main.h \
	main-widgets.h \
	mountlist.c mountlist.h \
	panelize.c panelize.h \
	subshell.c subshell.h \
	textconf.c textconf.h \
	treestore.c treestore.h \
	user.c user.h

EXTRA_DIST = man2hlp.c $(SRC_maintainer) $(SRC_charset)

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
