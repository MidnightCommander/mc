extfsdir = $(EXTFSHELPERSDIR)

# Files to install and distribute other than extfs scripts
EXTFS_MISC  = README README.extfs

# Scripts hat don't need adaptation to the local system
EXTFS_CONST = bpp changesetfs gitfs+ patchsetfs rpm trpm u7z

# Scripts that need adaptation to the local system - source files
EXTFS_IN    = 			\
	a+.in			\
	apt+.in			\
	audio.in		\
	brotli.in		\
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
	torrent.in		\
	uace.in			\
	ualz.in			\
	uar.in			\
	uarc.in			\
	uarj.in			\
	uc1541.in		\
	ucab.in			\
	uha.in			\
	ulha.in			\
	ulib.in			\
	unar.in			\
	urar.in			\
	usqfs.in		\
	uwim.in			\
	uzip.in			\
	uzoo.in

# Scripts that need adaptation to the local system - files to install
EXTFS_OUT = 			\
	a+			\
	apt+			\
	audio			\
	brotli			\
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
	torrent			\
	uace			\
	ualz			\
	uar			\
	uarc			\
	uarj			\
	uc1541			\
	ucab			\
	uha			\
	ulha			\
	ulib			\
	unar			\
	urar			\
	usqfs			\
	uwim			\
	uzip			\
	uzoo

if ENABLE_VFS_EXTFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST)
