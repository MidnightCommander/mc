AM_CFLAGS = $(GLIB_CFLAGS)
AM_CPPFLAGS = -DREGEX_MALLOC -DLIBDIR=\""$(mclibdir)/"\" \
	-DCONFDIR=\""$(sysconfdir)/"\" -DLOCALEDIR=\""$(localedir)"\"

mclibdir = $(libdir)/mc
suppbindir = $(mclibdir)/bin
localedir = $(datadir)/locale

bin_PROGRAMS = mc mcmfmt

if CONS_SAVER
suppbin_PROGRAMS = cons.saver
endif

noinst_PROGRAMS = man2hlp

mclib_DATA = mc.hlp

mcmfmt_SOURCES = mfmt.c

cons_saver_SOURCES = cons.saver.c

man2hlp_SOURCES = man2hlp.c

if INCLUDED_SLANG
SLANGLIB = ../slang/libmcslang.a
else
SLANGLIB = $(LSLANG)
endif

if USE_EDIT
EDITLIB = ../edit/libedit.a
endif

if USE_VFS
VFSLIB = ../vfs/libvfs-mc.a
endif

mc_LDADD = $(EDITLIB) $(SLANGLIB) $(VFSLIB) \
	$(INTLLIBS) $(LGPM) $(TERMNET) $(MCLIBS) $(LIBICONV)

CHARSET_SRC = charsets.c charsets.h selcodepage.c selcodepage.h

SRCS =	achown.c achown.h background.c background.h boxes.c boxes.h	\
	chmod.c chmod.h chown.c chown.h cmd.c cmd.h color.c color.h	\
	command.c command.h complete.c complete.h cons.handler.c	\
	cons.saver.h dialog.c dialog.h dir.c dir.h dlg.c dlg.h		\
	eregex.h ext.c ext.h file.c filegui.c filegui.h file.h		\
	filenot.c fileopctx.c fileopctx.h find.c find.h findme.c	\
	findme.h fs.h fsusage.c fsusage.h global.h help.c help.h	\
	hotlist.c hotlist.h i18n.h info.c info.h key.c key.h keys.h	\
	keyxdef.c layout.c layout.h learn.c learn.h listmode.c		\
	listmode.h mad.c mad.h main.c main.h mem.h menu.c menu.h	\
	mountlist.c mountlist.h mouse.c mouse.h myslang.h option.c	\
	option.h panel.h panelize.c panelize.h poptalloca.h popt.c	\
	poptconfig.c popt.h popthelp.c poptint.h poptparse.c profile.c	\
	profile.h regex.c rxvt.c screen.c setup.c setup.h slint.c	\
	subshell.c subshell.h terms.c text.c textconf.c textconf.h	\
	tree.c tree.h treestore.c treestore.h tty.h user.c user.h	\
	util.c util.h utilunix.c view.c view.h widget.c widget.h	\
	win.c win.h wtools.c wtools.h

if CHARSET
mc_SOURCES = $(SRCS) $(CHARSET_SRC)
else
mc_SOURCES = $(SRCS)
endif

EXTRA_DIST = TODO ChangeLog OChangeLog man2hlp.c \
	gindex.pl ncurses.patch mc.hlp $(CHARSET_SRC)

$(srcdir)/mc.hlp: $(top_srcdir)/doc/mc.1.in $(top_srcdir)/lib/xnc.hlp $(srcdir)/gindex.pl man2hlp.c
	- $(MAKE) man2hlp
	- perl $(srcdir)/gindex.pl $(top_srcdir)/doc/mc.1.in \
	       $(top_srcdir)/lib/xnc.hlp $(srcdir)/mc.hlp
	- touch $(srcdir)/mc.hlp

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
