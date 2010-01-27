if USE_SAMBA_FS
SAMBA_CFLAGS  = -DCONFIGDIR=\""@configdir@"\"
SAMBA_SUBDIRS = samba
endif

DIST_SUBDIRS = extfs

SUBDIRS   = extfs $(SAMBA_SUBDIRS)
AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(SAMBA_CFLAGS)

AM_CPPFLAGS = -DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

BASICFILES = 			\
	cpio.c			\
	direntry.c		\
	extfs.c 		\
	gc.c	 		\
	local.c 		\
	tar.c			\
	sfs.c			\
	utilvfs.c		\
	vfs.c

VFSHDRS = 			\
	fish.h			\
	ftpfs.h 		\
	gc.h	 		\
	local.h			\
	mcfs.h 			\
	mcfsutil.h 		\
	netutil.h 		\
	smbfs.h			\
	utilvfs.h		\
	vfs.h			\
	vfs-impl.h		\
	xdirentry.h

SMBFILES = smbfs.c
if USE_SAMBA_FS
SMB_NETFILES = $(SMBFILES)
endif

UNDELFILES = undelfs.c
if ENABLE_VFS_UNDELFS
UNDEL_FILES = $(UNDELFILES)
endif

NETFILES = netutil.c fish.c ftpfs.c mcfs.c mcfsutil.c $(SMB_NETFILES)

NONETFILES = $(BASICFILES) $(UNDEL_FILES)

EXTRA_DIST = HACKING README README.fish \
	$(VFSHDRS) $(BASICFILES) $(NETFILES) $(SMBFILES) $(UNDELFILES)

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

if ENABLE_VFS_NET
libvfs_mc_la_SOURCES = $(NETFILES) $(NONETFILES)
else
libvfs_mc_la_SOURCES = $(NONETFILES)
endif

if ENABLE_VFS
if ENABLE_MCSERVER
sbin_PROGRAMS = mcserv
mcserv_SOURCES = mcserv.c
mcserv_LDADD = $(MCSERVLIBS) $(GLIB_LIBS) libvfs-mc.la
endif
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

