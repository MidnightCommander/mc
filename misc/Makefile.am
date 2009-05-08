## Process this file with automake to create Makefile.in.


LIBFILES_OUT = mc.ext

LIBFILES_CONST =  mc.lib mc.menu mc.menu.sr cedit.menu edit.indent.rc	\
	edit.spell.rc

noinst_DATA = xterm.ad


pkgdata_DATA = $(LIBFILES_CONST) $(LIBFILES_ADD) $(LIBFILES_OUT)

CLEANFILES = $(SCRIPTS_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_IN) \
	$(noinst_DATA)

