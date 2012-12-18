DIST_SUBDIRS =

SUBDIRS = helpers

AM_CPPFLAGS = \
	-DCONFIGDIR=\""@smbconfigdir@"\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-smbfs.la

libvfs_smbfs_la_SOURCES = \
	smbfs.c smbfs.h

SAMBA_DIST =			\
	Makefile.in		\
	aclocal.m4		\
	configure.ac		\
	configure		\
	internals.doc		\
	parsing.doc		\
	include/byteorder.h	\
	include/charset.h	\
	include/client.h	\
	include/config.h.in	\
	include/includes.h	\
	include/kanji.h		\
	include/local.h		\
	include/nameserv.h	\
	include/nterr.h		\
	include/proto.h		\
	include/smb.h		\
	include/stamp-h.in	\
	include/trans2.h	\
	include/version.h	\
	lib/charcnv.c		\
	lib/charset.c		\
	lib/debug.c		\
	lib/interface.c		\
	lib/kanji.c		\
	lib/md4.c		\
	lib/netmask.c		\
	lib/slprintf.c		\
	lib/system.c		\
	lib/time.c		\
	lib/username.c		\
	lib/util.c		\
	lib/util_file.c		\
	lib/util_sock.c		\
	lib/util_str.c		\
	libsmb/clientgen.c	\
	libsmb/namequery.c	\
	libsmb/nmblib.c		\
	libsmb/nterr.c		\
	libsmb/pwd_cache.c	\
	libsmb/smbdes.c		\
	libsmb/smbencrypt.c	\
	libsmb/smberr.c		\
	param/loadparm.c	\
	param/params.c

dist-hook:
	$(mkinstalldirs) $(distdir)/helpers
	$(mkinstalldirs) $(distdir)/helpers/include
	$(mkinstalldirs) $(distdir)/helpers/lib
	$(mkinstalldirs) $(distdir)/helpers/libsmb
	$(mkinstalldirs) $(distdir)/helpers/param
	for I in $(SAMBA_DIST); do \
	  cp -p $(srcdir)/helpers/$$I $(distdir)/helpers/$$I || exit 1; \
	done

mostlyclean-local:
	if test -f helpers/Makefile; then \
	  (cd helpers && $(MAKE) mostlyclean) \
	else :; fi

clean-local:
	if test -f helpers/Makefile; then \
	  (cd helpers && $(MAKE) clean) \
	else :; fi

distclean-local:
	if test -f helpers/Makefile; then \
	  (cd helpers && $(MAKE) distclean) \
	else :; fi

