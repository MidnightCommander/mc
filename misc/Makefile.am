## Process this file with automake to create Makefile.in.

SUBDIRS = ext.d macros.d skins syntax

LIBFILES_OUT = mc.ext.ini

noinst_DATA = xterm.ad

PKGDATA_IN = mc.charsets.in

PKGDATA_OUT =
if CHARSET
PKGDATA_OUT += mc.charsets
endif

dist_pkgdata_DATA = \
	mc.lib

pkgdata_DATA = \
	$(PKGDATA_OUT)

SCRIPTS_IN = \
	mc.menu.in \
	mcedit.menu.in

SCRIPTS_OUT = \
	mc.menu \
	mcedit.menu

LIBFILES_CONST = \
	filehighlight.ini \
	mc.default.keymap \
	mc.emacs.keymap


LIBFILES_SCRIPT = \
	edit.indent.rc

CLEANFILES = $(SCRIPTS_OUT) $(PKGDATA_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_OUT) \
	$(LIBFILES_OUT)

cfg_SCRIPTS = \
	$(LIBFILES_SCRIPT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(LIBFILES_SCRIPT) \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(PKGDATA_IN)

install-data-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap && $(LN_S) mc.default.keymap mc.keymap

uninstall-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap
