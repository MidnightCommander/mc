SUBDIRS = extfs

AM_CFLAGS = $(GLIB_CFLAGS)

BASICFILES = 			\
	cpio.c			\
	direntry.c		\
	extfs.c 		\
	local.c 		\
	names.c			\
	tar.c			\
	sfs.c			\
	vfs.c

VFSHDRS = 			\
	container.h 		\
	extfs.h 		\
	fish.h			\
	ftpfs.h 		\
	local.h			\
	mcfs.h 			\
	names.h 		\
	smbfs.h			\
	tar.h 			\
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

NETFILES = tcputil.c fish.c ftpfs.c mcfs.c utilvfs.c $(SMB_NETFILES)

NONETFILES = $(BASICFILES) $(UNDEL_FILES)

EXTRA_DIST = ChangeLog README README.fish \
	$(VFSHDRS) $(BASICFILES) $(NETFILES) $(SMBFILES) $(UNDELFILES)

dist-hook:
	mkdir $(distdir)/samba
	mkdir $(distdir)/samba/include
	mkdir $(distdir)/samba/lib
	mkdir $(distdir)/samba/libsmb
	mkdir $(distdir)/samba/param
	mkdir $(distdir)/samba/tests
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

if MCSERV_INSTALL
sbin_PROGRAMS = mcserv
else
noinst_PROGRAMS = mcserv
endif

mcserv_SOURCES = mcserv.c tcputil.c

mcserv_LDFLAGS = $(SHADOWLIB) $(TERMNET) $(PAMLIBS) $(LCRYPT)

if USE_SAMBA_FS
libvfs-mc.a: $(libvfs_mc_a_OBJECTS) $(libvfs_mc_a_DEPENDENCIES) samba/libsamba.a
	rm -f $@ libtmp.a
	cp -f samba/libsamba.a libtmp.a
	$(libvfs_mc_a_AR) libtmp.a $(libvfs_mc_a_OBJECTS) $(libvfs_mc_a_LIBADD)
	mv libtmp.a $@
	$(RANLIB) $@
endif

samba/libsamba.a:
	cd samba && $(MAKE) libsamba.a

SAMBA_DIST =			\
	Makefile.in		\
	acconfig.h		\
	aclocal.m4		\
	architecture.doc	\
	configure.in		\
	configure		\
	internals.doc		\
	parsing.doc		\
	include/byteorder.h	\
	include/charset.h	\
	include/client.h	\
	include/config.h.in	\
	include/dlinklist.h	\
	include/includes.h	\
	include/kanji.h		\
	include/local.h		\
	include/nameserv.h	\
	include/ntdomain.h	\
	include/nterr.h		\
	include/proto.h		\
	include/rpc_dce.h	\
	include/rpc_lsa.h	\
	include/rpc_misc.h	\
	include/rpc_netlogon.h	\
	include/rpc_reg.h	\
	include/rpc_samr.h	\
	include/rpc_secdes.h	\
	include/rpc_srvsvc.h	\
	include/rpc_wkssvc.h	\
	include/rpcclient.h	\
	include/smb.h		\
	include/stamp-h.in	\
	include/trans2.h	\
	include/version.h	\
	lib/charcnv.c		\
	lib/charset.c		\
	lib/debug.c		\
	lib/doscalls.c		\
	lib/genrand.c		\
	lib/interface.c		\
	lib/kanji.c		\
	lib/md4.c		\
	lib/netmask.c		\
	lib/signal.c		\
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
	param/params.c		\
	tests/README		\
	tests/ftruncate.c	\
	tests/getgroups.c	\
	tests/summary.c		\
	tests/trivial.c

