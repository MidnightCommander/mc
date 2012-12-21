extfsdir = $(libexecdir)/@PACKAGE@/extfs.d

EXTFSCONFFILES = sfs.ini

# Files to install and distribute other than extfs scripts
EXTFS_MISC  = README README.extfs

# Scripts hat don't need adaptation to the local system
EXTFS_CONST = bpp changesetfs gitfs+ patchsetfs rpm trpm u7z

# Scripts that need adaptation to the local system - source files
EXTFS_IN    = 			\
	a+.in			\
	apt+.in			\
	audio.in		\
	deb.in			\
	deba.in			\
	debd.in			\
	dpkg+.in		\
	iso9660.in		\
	hp48+.in		\
	lslR.in			\
	mailfs.in		\
	patchfs.in		\
	rpms+.in		\
	s3+.in			\
	uace.in			\
	ualz.in			\
	uar.in			\
	uarc.in			\
	uarj.in			\
	uc1541.in		\
	ucab.in			\
	uha.in			\
	ulha.in			\
	urar.in			\
	uzip.in			\
	uzoo.in

# Scripts that need adaptation to the local system - files to install
EXTFS_OUT = 			\
	a+			\
	apt+			\
	audio			\
	deb			\
	deba			\
	debd			\
	dpkg+			\
	iso9660			\
	hp48+			\
	lslR			\
	mailfs			\
	patchfs			\
	rpms+			\
	s3+			\
	uace			\
	ualz			\
	uar			\
	uarc			\
	uarj			\
	uc1541			\
	ucab			\
	uha			\
	ulha			\
	urar			\
	uzip			\
	uzoo

if ENABLE_VFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)

extfsconfdir = $(sysconfdir)/@PACKAGE@
extfsconf_DATA = $(EXTFSCONFFILES)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST) $(EXTFSCONFFILES)
