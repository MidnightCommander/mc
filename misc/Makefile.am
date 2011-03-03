## Process this file with automake to create Makefile.in.

SUBDIRS = macros.d skins syntax

LIBFILES_OUT = mc.ext

noinst_DATA = xterm.ad

pkgdata_DATA = \
	mc.lib
if CHARSET
pkgdata_DATA += mc.charsets
endif

LIBFILES_CONST = \
	mcedit.menu      \
	edit.indent.rc \
	edit.spell.rc \
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
	$(LIBFILES_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(pkgdata_DATA)
#if CHARSET is defined, mc.charsets is already here
if !CHARSET
EXTRA_DIST += mc.charsets
endif
