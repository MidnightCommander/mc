SUBDIRS = filehighlight mcconfig search skin tty vfs strutil

noinst_LTLIBRARIES = libmc.la

SUBLIB_includes = \
	filehighlight.h \
	mcconfig.h \
	search.h \
	strescape.h \
	strutil.h

SRC_mc_utils = \
	utilunix.c \
	unixcompat.h \
	util.c util.h


libmc_la_SOURCES = \
	$(SUBLIB_includes) \
	$(SRC_mc_utils) \
	fileloc.h \
	fs.h \
	glibcompat.c glibcompat.h \
	global.h \
	timefmt.h

if USE_MAINTAINER_MODE
libmc_la_SOURCES += logging.c logging.h
endif


libmc_la_CFLAGS= $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

libmc_la_LIBADD = \
	filehighlight/libmcfilehighlight.la \
	mcconfig/libmcconfig.la \
	search/libsearch.la \
	strutil/libmcstrutil.la \
	skin/libmcskin.la \
	tty/libmctty.la

if ENABLE_VFS
libmc_la_LIBADD += vfs/mc-vfs/libvfs-mc.la
endif

