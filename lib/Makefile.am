mclibdir   = $(libdir)/mc
desktopdir = $(mclibdir)/desktop-scripts
suppbindir = $(mclibdir)/bin
tidir      = $(mclibdir)/term
confdir    = $(sysconfdir)

LOCALIZED_HELP = mc.hlp.hu
LIBFILES_OUT = mc.ext mc-gnome.ext
LIBFILES_CONST = mc.hint mc.hint.es mc.hint.cs mc.hint.it mc.hint.pl \
	mc.hint.ru mc.lib mc.menu cedit.menu edit.indent.rc

noinst_DATA = \
	xnc.hlp			\
	ncurses.h 		\
	mcserv.init 		\
	mcserv.pamd 		\
	mc.ext.in.qnx.diff 	\
	mc.global

noinst_SCRIPTS = tdiff

conf_DATA = mc.global
suppbin_SCRIPTS = mc.csh mc.sh
desktop_DATA = startup.links README.desktop
ti_DATA = README.xterm linux.ti xterm.ad xterm.ti ansi.ti vt100.ti xterm.tcap

mclib_DATA = $(LIBFILES_CONST) $(LIBFILES_OUT) $(LOCALIZED_HELP)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(LOCALIZED_HELP) \
	$(conf_DATA) \
	$(desktop_DATA) \
	$(ti_DATA) \
	$(noinst_DATA) \
	$(noinst_SCRIPTS)
