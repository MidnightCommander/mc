mclibdir   = $(libdir)/mc
desktopdir = $(mclibdir)/desktop-scripts
suppbindir = $(mclibdir)/bin
tidir      = $(mclibdir)/term
confdir    = $(sysconfdir)

LIBFILES_IN = mc.ext.in mc-gnome.ext.in
LIBFILES_OUT = mc.ext mc-gnome.ext
LIBFILES_CONST = mc.hint mc.hint.es mc.hint.cs mc.hint.ru \
	mc.lib mc.menu cedit.menu edit.indent.rc
SUPPBIN_IN = mc.csh.in mc.sh.in
SUPPBIN_OUT =  mc.csh mc.sh
DESKTOP_FILES = startup.links README.desktop
TIFILES = README.xterm linux.ti xterm.ad xterm.ti ansi.ti vt100.ti xterm.tcap
NOINST_FILES = tdiff		\
	tdiff		 	\
	xnc.hlp			\
	ncurses.h 		\
	mcserv.init 		\
	mcserv.pamd 		\
	mc.ext.in.qnx.diff 	\
	mc.global

conf_DATA = mc.global

# Files processed by configure don't need to be here
EXTRA_DIST = $(LIBFILES_CONST) $(TIFILES) $(DESKTOP_FILES) $(conf_DATA)

mclib_DATA = $(LIBFILES_CONST) $(LIBFILES_OUT)
suppbin_SCRIPTS = $(SUPPBIN_OUT)
ti_DATA = $(TIFILES)
desktop_DATA = $(DESKTOP_FILES)
