suppbindir = $(pkgdatadir)/bin
tidir      = $(pkgdatadir)/term

if CHARSET
LIBFILES_ADD = mc.charsets
else
LIBFILES_ADD =
endif

LIBFILES_OUT = mc.ext
LIBFILES_CONST = mc.hint mc.hint.cs mc.hint.es mc.hint.hu mc.hint.it	\
	mc.hint.nl mc.hint.pl mc.hint.ru mc.hint.sr mc.hint.uk		\
	mc.hint.zh mc.lib mc.menu mc.menu.sr cedit.menu edit.indent.rc	\
	edit.spell.rc

noinst_DATA = \
	mcserv.init 		\
	mcserv.pamd

noinst_SCRIPTS = tdiff

SCRIPTS_IN = mc.csh.in mc.sh.in mc-wrapper.csh.in mc-wrapper.sh.in
SCRIPTS_OUT = mc.csh mc.sh mc-wrapper.csh mc-wrapper.sh

suppbin_SCRIPTS = $(SCRIPTS_OUT)
ti_DATA = README.xterm linux.ti xterm.ad xterm.ti ansi.ti vt100.ti xterm.tcap

pkgdata_DATA = $(LIBFILES_CONST) $(LIBFILES_ADD) $(LIBFILES_OUT)

CLEANFILES = $(SCRIPTS_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_IN) \
	$(ti_DATA) \
	$(noinst_DATA) \
	$(noinst_SCRIPTS) \
	mc.charsets

mc.csh: $(srcdir)/mc.csh.in
	sed "s%@""suppbindir@%$(suppbindir)%" $(srcdir)/mc.csh.in > mc.csh

mc.sh: $(srcdir)/mc.sh.in
	sed "s%@""suppbindir@%$(suppbindir)%" $(srcdir)/mc.sh.in > mc.sh

mc-wrapper.csh: $(srcdir)/mc-wrapper.csh.in
	sed "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.csh.in > mc-wrapper.csh

mc-wrapper.sh: $(srcdir)/mc-wrapper.sh.in
	sed "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.sh.in > mc-wrapper.sh

