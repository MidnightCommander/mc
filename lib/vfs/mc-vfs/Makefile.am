if USE_SAMBA_FS
SAMBA_CFLAGS  = -DCONFIGDIR=\""@configdir@"\"
SAMBA_SUBDIRS = samba
endif

DIST_SUBDIRS = extfs fish

SUBDIRS   = extfs fish $(SAMBA_SUBDIRS)
AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(SAMBA_CFLAGS)

AM_CPPFLAGS = -DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

BASICFILES = \
	vfs.c vfs.h vfs-impl.h	\
	direntry.c xdirentry.h	\
	utilvfs.c utilvfs.h	\
	local.c local.h		\
	gc.c gc.h		\
	extfs.c

CPIOFILES = cpio.c
TARFILES = tar.c
SFSFILES = sfs.c
EXTFSFILES = extfs.c
UNDELFILES = undelfs.c

NETFILES = \
	netutil.c netutil.h	\
	fish.c fish.h fishdef.h	\
	ftpfs.c ftpfs.h

SMBFILES = smbfs.c smbfs.h

libvfs_mc_la_SOURCES = $(BASICFILES)
if ENABLE_VFS_CPIO
libvfs_mc_la_SOURCES += $(CPIOFILES)
endif
if ENABLE_VFS_TAR
libvfs_mc_la_SOURCES += $(TARFILES)
endif
if ENABLE_VFS_SFS
libvfs_mc_la_SOURCES += $(SFSFILES)
endif
if ENABLE_VFS_UNDELFS
libvfs_mc_la_SOURCES += $(UNDELFILES)
endif
if ENABLE_VFS_NET
libvfs_mc_la_SOURCES += $(NETFILES)
endif
if USE_SAMBA_FS
libvfs_mc_la_SOURCES += $(SMBFILES)
endif

EXTRA_DIST = HACKING README \
	$(BASICFILES) \
	$(CPIOFILES) \
	$(TARFILES) \
	$(SFSFILES) \
	$(UNDELFILES) \
	$(NETFILES) \
	$(SMBFILES)

dist-hook:
	$(mkinstalldirs) $(distdir)/samba
	$(mkinstalldirs) $(distdir)/samba/include
	$(mkinstalldirs) $(distdir)/samba/lib
	$(mkinstalldirs) $(distdir)/samba/libsmb
	$(mkinstalldirs) $(distdir)/samba/param
	for I in $(SAMBA_DIST); do \
	  cp -p $(srcdir)/samba/$$I $(distdir)/samba/$$I || exit 1; \
	done

mostlyclean-local:
	if test -f samba/Makefile; then \
	  (cd samba && $(MAKE) mostlyclean) \
	else :; fi

clean-local:
	if test -f samba/Makefile; then \
	  (cd samba && $(MAKE) clean) \
	else :; fi

distclean-local:
	if test -f samba/Makefile; then \
	  (cd samba && $(MAKE) distclean) \
	else :; fi

if ENABLE_VFS
noinst_LTLIBRARIES = libvfs-mc.la
else
noinst_LTLIBRARIES =
endif

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
