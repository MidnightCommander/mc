SUBDIRS = filehighlight mcconfig search skin tty vfs

noinst_LTLIBRARIES = libmc.la

libmc_la_SOURCES =

libmc_la_CFLAGS=

libmc_la_LIBADD = \
	filehighlight/libmcfilehighlight.la \
	mcconfig/libmcconfig.la \
	search/libsearch.la \
	skin/libmcskin.la \
	tty/libmctty.la

if ENABLE_VFS
libmc_la_LIBADD+= vfs/mc-vfs/libvfs-mc.a
if USE_SAMBA_FS
libmc_la_LIBADD+= vfs/mc-vfs/samba/libsamba.a
endif
endif

