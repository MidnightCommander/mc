mclibdir   = $(libdir)/mc
suppbindir = $(mclibdir)/bin
tidir      = $(mclibdir)/term

if CHARSET
LIBFILES_ADD = mc.charsets
else
LIBFILES_ADD =
endif

LIBFILES_OUT = mc.ext
LIBFILES_CONST = mc.hint mc.hint.cs mc.hint.es mc.hint.hu mc.hint.it \
	mc.hint.nl mc.hint.pl mc.hint.ru mc.hint.uk mc.hint.zh mc.lib \
	mc.menu cedit.menu edit.indent.rc edit.spell.rc

noinst_DATA = \
	mcserv.init 		\
	mcserv.pamd

noinst_SCRIPTS = tdiff

suppbin_SCRIPTS = mc.csh mc.sh
ti_DATA = README.xterm linux.ti xterm.ad xterm.ti ansi.ti vt100.ti xterm.tcap

mclib_DATA = $(LIBFILES_CONST) $(LIBFILES_ADD) $(LIBFILES_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(ti_DATA) \
	$(noinst_DATA) \
	$(noinst_SCRIPTS) \
	mc.charsets

