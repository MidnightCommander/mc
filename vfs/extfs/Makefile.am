extfsdir = $(pkgdatadir)/extfs

EXTFSCONFFILES = extfs.ini sfs.ini

# Files to install and distribute other than extfs scripts
EXTFS_MISC  = README

# Scripts hat don't need adaptation to the local system
EXTFS_CONST = bpp rpm trpm u7z ucab

# Scripts that need adaptation to the local system - source files
EXTFS_IN    = 			\
	a.in			\
	apt.in			\
	audio.in		\
	deb.in			\
	deba.in			\
	debd.in			\
	dpkg.in			\
	iso9660.in		\
	hp48.in			\
	lslR.in			\
	mailfs.in		\
	patchfs.in		\
	rpms.in			\
	uace.in			\
	ualz.in			\
	uar.in			\
	uarc.in			\
	uarj.in			\
	uc1541.in		\
	uha.in			\
	ulha.in			\
	urar.in			\
	uzip.in			\
	uzoo.in

# Scripts that need adaptation to the local system - files to install
EXTFS_OUT = 			\
	a			\
	apt			\
	audio			\
	deb			\
	deba			\
	debd			\
	dpkg			\
	iso9660			\
	hp48			\
	lslR			\
	mailfs			\
	patchfs			\
	rpms			\
	uace			\
	ualz			\
	uar			\
	uarc			\
	uarj			\
	uc1541			\
	uha			\
	ulha			\
	urar			\
	uzip			\
	uzoo

if USE_VFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)

extfsconfdir = $(sysconfdir)/@PACKAGE@/extfs
extfsconf_DATA = $(EXTFSCONFFILES)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST) $(EXTFSCONFFILES)

install-exec-local:
	$(MKDIR_P) $(DESTDIR)$(extfsdir)
	$(LN_S) rpm $(DESTDIR)$(extfsdir)/srpm

uninstall-local:
	-rm $(DESTDIR)$(extfsdir)/srpm
