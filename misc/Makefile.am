## Process this file with automake to create Makefile.in.

SUBDIRS = ext.d macros.d skins syntax

LIBFILES_OUT = mc.ext

noinst_DATA = xterm.ad

pkgdata_DATA = \
	mc.lib

SCRIPTS_IN = \
	mc.menu.in \
	mcedit.menu.in
if CHARSET
SCRIPTS_IN += mc.charsets.in
endif

SCRIPTS_OUT = \
	mc.menu \
	mcedit.menu
if CHARSET
SCRIPTS_OUT += mc.charsets
endif

LIBFILES_CONST = \
	filehighlight.ini \
	mc.default.keymap \
	mc.emacs.keymap


LIBFILES_SCRIPT = \
	edit.indent.rc

CLEANFILES = $(SCRIPTS_OUT)

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
	$(pkgdata_DATA)

install-data-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap && $(LN_S) mc.default.keymap mc.keymap

uninstall-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap
