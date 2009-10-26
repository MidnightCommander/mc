## Process this file with automake to create Makefile.in.

SUBDIRS = skins

LIBFILES_OUT = mc.ext

noinst_DATA = xterm.ad

pkgdata_DATA =

LIBFILES_CONST = \
	cedit.menu      \
	edit.indent.rc \
	edit.spell.rc \
	mc.lib \
	filehighlight.ini \
	mc.keymap \
	mc.keymap.default \
	mc.keymap.emacs \
	mc.menu \
	mc.menu.sr


CLEANFILES = $(SCRIPTS_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_CONST) \
	$(LIBFILES_OUT) \
	$(LIBFILES_ADD)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(pkgdata_DATA)

