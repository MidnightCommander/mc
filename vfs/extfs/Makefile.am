extfsdir = $(pkgdatadir)/extfs

# Files to install and distribute other than extfs scripts
EXTFS_MISC  = README extfs.ini sfs.ini

# Scripts hat don't need adaptation to the local system
EXTFS_CONST = bpp rpm trpm

# Scripts that need adaptation to the local system - source files
EXTFS_IN    = 			\
	a.in			\
	apt.in			\
	audio.in		\
	deb.in			\
	deba.in			\
	debd.in			\
	dpkg.in			\
	hp48.in			\
	lslR.in			\
	mailfs.in		\
	patchfs.in		\
	rpms.in			\
	uar.in			\
	uarj.in			\
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
	hp48			\
	lslR			\
	mailfs			\
	patchfs			\
	rpms			\
	uar			\
	uarj			\
	uha			\
	ulha			\
	urar			\
	uzip			\
	uzoo

if USE_VFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST)

