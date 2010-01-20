SUBDIRS = filehighlight mcconfig search skin tty vfs

noinst_LTLIBRARIES = libmc.la

libmc_la_SOURCES = \
	fs.h \
	glibcompat.c glibcompat.h \
	global.h


libmc_la_CFLAGS= $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

libmc_la_LIBADD = \
	filehighlight/libmcfilehighlight.la \
	mcconfig/libmcconfig.la \
	search/libsearch.la \
	skin/libmcskin.la \
	tty/libmctty.la \
	$(INTLLIBS) $(MCLIBS) $(SLANGLIB) $(LIBICONV) $(GLIB_LIBS)

if ENABLE_VFS
libmc_la_LIBADD += vfs/mc-vfs/libvfs-mc.la
endif

