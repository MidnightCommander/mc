
/** \file vfsdummy.h
 *  \brief Header: replacement for vfs.h if VFS support is disabled
 */

#ifndef MC_VFSDUMMY_H
#define MC_VFSDYMMY_H

#include "global.h"		/* glib.h*/
#include "util.h"

/* Flags of VFS classes */
#define VFSF_LOCAL 1		/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2		/* Hard links not supported */

#define O_LINEAR 0

#define mc_close close
#define mc_lseek lseek
#define mc_opendir opendir
#define mc_readdir readdir
#define mc_closedir closedir

#define mc_stat stat
#define mc_mknod mknod
#define mc_link link
#define mc_mkdir mkdir
#define mc_rmdir rmdir
#define mc_fstat fstat
#define mc_lstat lstat

#define mc_readlink readlink
#define mc_symlink symlink
#define mc_rename rename

#define mc_open open
#define mc_utime utime
#define mc_chmod chmod
#define mc_chown chown
#define mc_chdir chdir
#define mc_unlink unlink

static inline int
return_zero (void)
{
    return 0;
}

#define mc_ctl(a,b,c) return_zero()
#define mc_setctl(a,b,c) return_zero()

#define mc_get_current_wd(x,size) get_current_wd (x, size)
#define mc_getlocalcopy(x) vfs_canon(x)
#define mc_ungetlocalcopy(x,y,z) do { } while (0)

#define vfs_strip_suffix_from_filename(x) g_strdup(x)

#define vfs_file_class_flags(x) (VFSF_LOCAL)
#define vfs_get_class(x) (struct vfs_class *)(NULL)

#define vfs_translate_url(s) g_strdup(s)
#define vfs_release_path(x)
#define vfs_add_current_stamps() do { } while (0)
#define vfs_timeout_handler() do { } while (0)
#define vfs_timeouts() 0

static inline char *
vfs_canon (const char *path)
{
    char *p = g_strdup (path);
    canonicalize_pathname(p);
    return p;
}

#endif				/* MC_VFSDUMMY_H */
