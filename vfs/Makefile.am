if USE_SAMBA_FS
SUBDIRS = samba extfs
else
SUBDIRS = extfs
endif

DIST_SUBDIRS = extfs

if USE_SAMBA_FS
AM_CFLAGS = $(GLIB_CFLAGS) -DCONFIGDIR=\""@configdir@"\"
else
AM_CFLAGS = $(GLIB_CFLAGS)
endif

BASICFILES = 			\
	cpio.c			\
	direntry.c		\
	extfs.c 		\
	local.c 		\
	tar.c			\
	sfs.c			\
	utilvfs.c		\
	vfs.c

VFSHDRS = 			\
	extfs.h 		\
	ftpfs.h 		\
	local.h			\
	mcfs.h 			\
	mcfsutil.h 		\
	smbfs.h			\
	tcputil.h 		\
	utilvfs.h		\
	vfs.h			\
	xdirentry.h

SMBFILES = smbfs.c
if USE_SAMBA_FS
SMB_NETFILES = $(SMBFILES)
endif

UNDELFILES = undelfs.c
if USE_UNDEL_FS
UNDEL_FILES = $(UNDELFILES)
endif

NETFILES = tcputil.c fish.c ftpfs.c mcfs.c mcfsutil.c $(SMB_NETFILES)

NONETFILES = $(BASICFILES) $(UNDEL_FILES)

EXTRA_DIST = ChangeLog README README.fish \
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

if USE_VFS
noinst_LIBRARIES = libvfs-mc.a
else
noinst_LIBRARIES =
endif

if USE_VFS_NET
libvfs_mc_a_SOURCES = $(NETFILES) $(NONETFILES)
else
libvfs_mc_a_SOURCES = $(NONETFILES)
endif

if USE_MCFS
sbin_PROGRAMS = mcserv
endif

mcserv_SOURCES = mcserv.c mcfsutil.c

mcserv_LDADD = $(MCSERVLIBS)

SAMBA_DIST =			\
	Makefile.in		\
	aclocal.m4		\
	configure.in		\
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
	lib/genrand.c		\
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
	lib/util_unistr.c	\
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

