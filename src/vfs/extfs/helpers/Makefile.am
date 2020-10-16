extfsdir = $(libexecdir)/@PACKAGE@/extfs.d

# Files to install and distribute other than extfs scripts
EXTFS_MISC  = README README.extfs

# Scripts hat don't need adaptation to the local system
EXTFS_CONST = bpp changesetfs gitfs+ patchsetfs rpm trpm u7z uc1541

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
	ucab.in			\
	uha.in			\
	ulha.in			\
	ulib.in			\
	unar.in			\
	urar.in			\
	uwim.in			\
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
	ucab			\
	uha			\
	ulha			\
	ulib			\
	unar			\
	urar			\
	uwim			\
	uzip			\
	uzoo

if ENABLE_VFS_EXTFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST)
