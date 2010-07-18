SUBDIRS = man2hlp viewer

if USE_EDIT
SUBDIRS += editor
endif

if USE_DIFF
SUBDIRS += diffviewer
endif

AM_CPPFLAGS = -DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\"


if CONS_SAVER
SUBDIRS += consaver
AM_CPPFLAGS += -DSAVERDIR=\""$(pkglibexecdir)"\"
endif

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

localedir = $(datadir)/locale
pkglibexecdir =		$(libexecdir)/@PACKAGE@

bin_PROGRAMS = mc mcmfmt

mcmfmt_SOURCES = mfmt.c

if USE_EDIT
EDITLIB = editor/libedit.la
endif

if USE_DIFF
DIFFLIB = diffviewer/libdiffviewer.la
endif

SRC_charset = charsets.c charsets.h selcodepage.c selcodepage.h

if CHARSET
    SRC_USE_charset=$(SRC_charset)
endif

mc_LDADD = \
	viewer/libmcviewer.la \
	$(DIFFLIB) $(EDITLIB) \
	../lib/libmc.la \
	$(MCLIBS) $(SLANGLIB) $(GLIB_LIBS) \
	$(PCRE_LIBS) $(LIBICONV) $(INTLLIBS)

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
mc_LDADD += ../lib/vfs/mc-vfs/samba/libsamba.a
endif

SRC_mc_widgets = \
	dialog.c dialog.h \
	dialog-switch.c dialog-switch.h \
	menu.c menu.h \
	screen.c panel.h \
	tree.c tree.h \
	widget.c widget.h \
	wtools.c wtools.h

SRC_mc_conssaver = \
	cons.handler.c consaver/cons.saver.h

SRC_mc_options = \
	learn.c learn.h \
	option.c option.h \
	setup.c setup.h

SRC_mc_keybind = \
	keybind.c keybind.h \
	cmddef.h

mc_SOURCES = \
	$(SRC_USE_charset) \
	$(SRC_mc_widgets) \
	$(SRC_mc_conssaver) \
	$(SRC_mc_options) \
	$(SRC_mc_keybind) \
	achown.c achown.h \
	args.c args.h \
	background.c background.h \
	boxes.c boxes.h \
	chmod.c chmod.h \
	chown.c chown.h \
	clipboard.c clipboard.h \
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
	panelize.c panelize.h \
	subshell.c subshell.h \
	textconf.c textconf.h \
	treestore.c treestore.h \
	user.c user.h

#if USE_MOUNTLIST
mc_SOURCES += mountlist.c mountlist.h
#endif

EXTRA_DIST = $(SRC_maintainer) $(SRC_charset)

# end of automated testing

install-exec-hook:
	$(MAKE) install_mcview
if USE_EDIT
	$(MAKE) install_mcedit
endif
if USE_DIFF
	$(MAKE) install_mcdiff
endif

#
# Make relative symlinks, but do the right thing if LN_S is `ln' or `cp'.
#
install_mcview:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcview && $(LN_S) mc mcview

install_mcedit:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcedit && $(LN_S) mc mcedit

install_mcdiff:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcdiff && $(LN_S) mc mcdiff

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcview
if USE_EDIT
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcedit
endif
if USE_DIFF
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcdiff
endif
