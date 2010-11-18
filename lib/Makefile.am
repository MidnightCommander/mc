SUBDIRS = filehighlight mcconfig search skin tty vfs strutil widget

noinst_LTLIBRARIES = libmc.la

SUBLIB_includes = \
	filehighlight.h \
	mcconfig.h \
	search.h \
	skin.h \
	strescape.h \
	strutil.h \
	widget.h

SRC_mc_utils = \
	utilunix.c \
	unixcompat.h \
	util.c util.h


libmc_la_SOURCES = \
	$(SUBLIB_includes) \
	$(SRC_mc_utils) \
	fileloc.h \
	fs.h \
	hook.c hook.h \
	glibcompat.c glibcompat.h \
	global.h \
	keybind.c keybind.h \
	lock.c lock.h \
	timefmt.c timefmt.h

if USE_MAINTAINER_MODE
libmc_la_SOURCES += logging.c logging.h
endif

if CHARSET
libmc_la_SOURCES += charsets.c charsets.h
endif

libmc_la_CFLAGS= $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

libmc_la_LIBADD = \
	filehighlight/libmcfilehighlight.la \
	mcconfig/libmcconfig.la \
	search/libsearch.la \
	strutil/libmcstrutil.la \
	skin/libmcskin.la \
	tty/libmctty.la \
	vfs/mc-vfs/libvfs-mc.la \
	widget/libmcwidget.la
