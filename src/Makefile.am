AM_CFLAGS = @GLIB_CFLAGS@
AM_CPPFLAGS = -DREGEX_MALLOC -DLIBDIR=\""$(mclibdir)/"\" \
	-DCONFDIR=\""$(confdir)/"\" -DLOCALEDIR=\""$(localedir)/"\"

mclibdir = $(libdir)/mc
suppbindir = $(mclibdir)/bin

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
SLANGLIB = @LSLANG@
endif

if USE_EDIT
EDITLIB = ../edit/libedit.a
endif

if USE_VFS
VFSLIB = ../vfs/libvfs-mc.a
endif

mc_LDADD = $(EDITLIB) $(SLANGLIB) $(VFSLIB) \
	@LINTL@ @LGPM@ @TERMNET@ @MCLIBS@ @LIBICONV@

if CHARSET
CHARSET_SRC = charsets.c selcodepage.c
else
CHARSET_SRC =
endif

SRCS =	dir.c util.c main.c screen.c dialog.c key.c keyxdef.c menu.c	\
	file.c win.c color.c help.c find.c profile.c user.c view.c	\
	ext.c mouse.c setup.c dlg.c option.c info.c			\
	tree.c widget.c chmod.c mad.c					\
	wtools.c cons.handler.c chown.c subshell.c terms.c boxes.c	\
	hotlist.c achown.c layout.c fsusage.c mountlist.c regex.c	\
	complete.c slint.c command.c cmd.c panelize.c learn.c		\
	listmode.c utilunix.c background.c rxvt.c			\
	text.c popt.c findme.c poptparse.c poptconfig.c popthelp.c	\
	filegui.c filenot.c fileopctx.c treestore.c textconf.c		\
	charsets.c selcodepage.c

HDRS =	color.h file.h mouse.h user.h dialog.h find.h main.h		\
	util.h dir.h global.h menu.h panel.h win.h mem.h		\
	help.h profile.h dlg.h option.h tree.h info.h			\
	widget.h chmod.h cons.saver.h mad.h wtools.h chown.h		\
	subshell.h view.h setup.h key.h ext.h boxes.h			\
	hotlist.h layout.h fsusage.h mountlist.h eregex.h complete.h	\
	myslang.h command.h cmd.h tty.h fs.h panelize.h achown.h	\
	learn.h listmode.h background.h					\
	textconf.h i18n.h findme.h popt.h filegui.h keys.h		\
	poptint.h poptalloca.h fileopctx.h treestore.h			\
	charsets.h selcodepage.h

mc_SOURCES = $(SRCS) $(HDRS) $(CHARSET_SRC)

EXTRA_DIST = TODO ChangeLog OChangeLog man2hlp.c \
	gindex.pl ncurses.patch mc.hlp

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
