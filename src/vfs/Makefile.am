noinst_LTLIBRARIES = libmc-vfs.la

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
libmc_vfs_la_SOURCES = plugins_init.c plugins_init.h

SUBDIRS = local
libmc_vfs_la_LIBADD = local/libvfs-local.la

if ENABLE_VFS_CPIO
SUBDIRS += cpio
libmc_vfs_la_LIBADD += cpio/libvfs-cpio.la
endif

if ENABLE_VFS_EXTFS
SUBDIRS += extfs
libmc_vfs_la_LIBADD += extfs/libvfs-extfs.la
endif

if ENABLE_VFS_FISH
SUBDIRS += fish
libmc_vfs_la_LIBADD += fish/libvfs-fish.la
endif

if ENABLE_VFS_FTP
SUBDIRS += ftpfs
libmc_vfs_la_LIBADD += ftpfs/libvfs-ftpfs.la
endif

if ENABLE_VFS_SFTP
SUBDIRS += sftpfs
libmc_vfs_la_LIBADD += sftpfs/libvfs-sftpfs.la
endif

if ENABLE_VFS_SFS
SUBDIRS += sfs
libmc_vfs_la_LIBADD += sfs/libvfs-sfs.la
endif

if ENABLE_VFS_SMB
SUBDIRS += smbfs
libmc_vfs_la_LIBADD += smbfs/libvfs-smbfs.la
endif

if ENABLE_VFS_TAR
SUBDIRS += tar
libmc_vfs_la_LIBADD += tar/libvfs-tar.la
endif

if ENABLE_VFS_UNDELFS
SUBDIRS += undelfs
libmc_vfs_la_LIBADD += undelfs/libvfs-undelfs.la
endif
