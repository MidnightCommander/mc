extfsdir = $(pkgdatadir)/extfs

# Files to install and distribute, but not filters
EXTFS_MISC  = README extfs.ini unarj.diff sfs.ini

# Filters that don't need adaptation to the local system
EXTFS_CONST = a apt audio deba debd dpkg hp48 mailfs patchfs rpm bpp \
	rpms trpm uarj

# Filters that need adaptation to the local system - source files
EXTFS_IN    = 			\
	deb.in 			\
	lslR.in 		\
	urar.in 		\
	uzip.in 		\
	uzoo.in 		\
	uar.in 			\
	ulha.in 		\
	uha.in

# Filters that need adaptation to the local system - files to install
EXTFS_OUT = 			\
	deb 			\
	lslR 			\
	urar 			\
	uzip 			\
	uzoo 			\
	uar 			\
	ulha 			\
	uha

if USE_VFS
extfs_DATA = $(EXTFS_MISC)
extfs_SCRIPTS = $(EXTFS_CONST) $(EXTFS_OUT)
endif

EXTRA_DIST = $(EXTFS_MISC) $(EXTFS_CONST)

