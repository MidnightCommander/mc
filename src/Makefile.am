SUBDIRS = filemanager man2hlp vfs viewer

if USE_EDIT
SUBDIRS += editor
endif

if USE_DIFF
SUBDIRS += diffviewer
endif

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

if CONS_SAVER
SUBDIRS += consaver
AM_CPPFLAGS += -DSAVERDIR=\""$(pkglibexecdir)"\"
endif

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

localedir = $(datadir)/locale
pkglibexecdir = $(libexecdir)/@PACKAGE@

bin_PROGRAMS = mc

if USE_EDIT
EDITLIB = editor/libedit.la
endif

if USE_DIFF
DIFFLIB = diffviewer/libdiffviewer.la
endif

mc_LDADD = \
	vfs/libmc-vfs.la \
	viewer/libmcviewer.la \
	filemanager/libmcfilemanager.la \
	$(DIFFLIB) $(EDITLIB) \
	$(top_builddir)/lib/libmc.la

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
mc_LDADD += vfs/smbfs/helpers/libsamba.a
endif

SRC_mc_conssaver = \
	cons.handler.c consaver/cons.saver.h

mc_SOURCES = \
	$(SRC_mc_conssaver) \
	args.c args.h \
	background.c background.h \
	clipboard.c clipboard.h \
	events_init.c events_init.h \
	execute.c execute.h \
	help.c help.h \
	history.h \
	keybind-defaults.c keybind-defaults.h \
	learn.c learn.h \
	main.c main.h \
	setup.c setup.h \
	subshell.c subshell.h \
	textconf.c textconf.h \
	util.c util.h

if CHARSET
    mc_SOURCES += selcodepage.c selcodepage.h
endif

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
