SUBDIRS = filehighlight mcconfig search skin tty vfs strutil

noinst_LTLIBRARIES = libmc.la


SRC_mc_utils = \
	utilunix.c \
	unixcompat.h \
	util.c util.h


libmc_la_SOURCES = \
	$(SRC_mc_utils) \
	fileloc.h \
	fs.h \
	glibcompat.c glibcompat.h \
	global.h \
	timefmt.h


libmc_la_CFLAGS= $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

libmc_la_LIBADD = \
	filehighlight/libmcfilehighlight.la \
	mcconfig/libmcconfig.la \
	search/libsearch.la \
	strutil/libmcstrutil.la \
	skin/libmcskin.la \
	tty/libmctty.la \
	$(INTLLIBS) $(MCLIBS) $(SLANGLIB) $(LIBICONV) $(GLIB_LIBS)

if ENABLE_VFS
libmc_la_LIBADD += vfs/mc-vfs/libvfs-mc.la
endif

