if ENABLE_VFS_SMB
SAMBA_CFLAGS  = -DCONFIGDIR=\""@smbconfigdir@"\"
SAMBA_SUBDIRS = samba
endif

DIST_SUBDIRS = extfs fish

SUBDIRS = $(SAMBA_SUBDIRS)
if ENABLE_VFS_EXTFS
SUBDIRS += extfs
endif
if ENABLE_VFS_FISH
SUBDIRS += fish
endif

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(SAMBA_CFLAGS)

AM_CPPFLAGS = -DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

BASICFILES = \
	vfs.c vfs.h vfs-impl.h	\
	direntry.c xdirentry.h	\
	parse_ls_vga.c \
	utilvfs.c utilvfs.h	\
	gc.c gc.h		\
	local.c local.h

CPIOFILES = cpio.c
TARFILES = tar.c
SFSFILES = sfs.c
EXTFSFILES = extfs.c
UNDELFILES = undelfs.c

NETFILES = netutil.c netutil.h

FTPFILES = ftpfs.c ftpfs.h
FISHFILES = fish.c fish.h fishdef.h
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
if ENABLE_VFS_EXTFS
libvfs_mc_la_SOURCES += $(EXTFSFILES)
endif
if ENABLE_VFS_UNDELFS
libvfs_mc_la_SOURCES += $(UNDELFILES)
endif
if ENABLE_VFS_NET
libvfs_mc_la_SOURCES += $(NETFILES)
endif
if ENABLE_VFS_FTP
libvfs_mc_la_SOURCES += $(FTPFILES)
endif
if ENABLE_VFS_FISH
libvfs_mc_la_SOURCES += $(FISHFILES)
endif
if ENABLE_VFS_SMB
libvfs_mc_la_SOURCES += $(SMBFILES)
endif

EXTRA_DIST = HACKING README \
	$(BASICFILES) \
	$(CPIOFILES) \
	$(TARFILES) \
	$(SFSFILES) \
	$(EXTFSFILES) \
	$(UNDELFILES) \
	$(NETFILES) \
	$(FTPFILES) \
	$(FISHFILES) \
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

noinst_LTLIBRARIES = libvfs-mc.la

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
