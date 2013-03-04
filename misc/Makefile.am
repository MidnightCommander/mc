## Process this file with automake to create Makefile.in.

SUBDIRS = ext.d macros.d skins syntax

LIBFILES_OUT = mc.ext

noinst_DATA = xterm.ad

pkgdata_DATA = \
	mc.lib
if CHARSET
pkgdata_DATA += mc.charsets
endif

LIBFILES_CONST = \
	mcedit.menu \
	filehighlight.ini \
	mc.keymap \
	mc.default.keymap \
	mc.emacs.keymap \
	mc.menu \
	mc.menu.sr

LIBFILES_SCRIPT = \
	edit.indent.rc

CLEANFILES = $(SCRIPTS_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_CONST) \
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
#if CHARSET is defined, mc.charsets is already here
if !CHARSET
EXTRA_DIST += mc.charsets
endif
