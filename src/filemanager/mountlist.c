/*
   Return a list of mounted file systems

   Copyright (C) 1991, 1992, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation, Inc.

   Copyright (C) 1991, 1992, 2011
   The Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file mountlist.c
 *  \brief Source: list of mounted filesystems
 */

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>             /* SIZE_MAX */
#include <sys/types.h>

#include <errno.h>

/* This header needs to be included before sys/mount.h on *BSD */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined STAT_STATVFS || defined STAT_STATVFS64      /* POSIX 1003.1-2001 (and later) with XSI */
#include <sys/statvfs.h>
#else
/* Don't include backward-compatibility files unless they're needed.
   Eventually we'd like to remove all this cruft.  */
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef MOUNTED_GETFSSTAT        /* OSF_1 and Darwin1.3.x */
#ifdef HAVE_SYS_UCRED_H
#include <grp.h>                /* needed on OSF V4.0 for definition of NGROUPS,
                                   NGROUPS is used as an array dimension in ucred.h */
#include <sys/ucred.h>          /* needed by powerpc-apple-darwin1.3.7 */
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>       /* needed by powerpc-apple-darwin1.3.7 */
#endif
#ifdef HAVE_STRUCT_FSSTAT_F_FSTYPENAME
#define FS_TYPE(Ent) ((Ent).f_fstypename)
#else
#define FS_TYPE(Ent) mnt_names[(Ent).f_type]
#endif
#endif /* MOUNTED_GETFSSTAT */
#endif /* STAT_STATVFS || STAT_STATVFS64 */

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_FS_S5PARAM_H    /* Fujitsu UXP/V */
#include <sys/fs/s5param.h>
#endif
#if defined HAVE_SYS_FILSYS_H && !defined _CRAY
#include <sys/filsys.h>         /* SVR2 */
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_DUSTAT_H            /* AIX PS/2 */
#include <sys/dustat.h>
#endif

#ifdef MOUNTED_GETMNTENT1       /* 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
#include <mntent.h>
#ifndef MOUNTED
#ifdef _PATH_MOUNTED            /* GNU libc  */
#define MOUNTED _PATH_MOUNTED
#endif
#ifdef MNT_MNTTAB               /* HP-UX.  */
#define MOUNTED MNT_MNTTAB
#endif
#ifdef MNTTABNAME               /* Dynix.  */
#define MOUNTED MNTTABNAME
#endif
#endif
#endif

#ifdef MOUNTED_GETMNTINFO       /* 4.4BSD.  */
#include <sys/mount.h>
#endif

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD 3.0.  */
#include <sys/statvfs.h>
#endif

#ifdef MOUNTED_GETMNT           /* Ultrix.  */
#include <sys/mount.h>
#include <sys/fs_types.h>
#endif

#ifdef MOUNTED_FS_STAT_DEV      /* BeOS.  */
#include <fs_info.h>
#include <dirent.h>
#endif

#ifdef MOUNTED_FREAD            /* SVR2.  */
#include <mnttab.h>
#endif

#ifdef MOUNTED_FREAD_FSTYP      /* SVR3.  */
#include <mnttab.h>
#include <sys/fstyp.h>
#include <sys/statfs.h>
#endif

#ifdef MOUNTED_LISTMNTENT
#include <mntent.h>
#endif

#ifdef MOUNTED_GETMNTENT2       /* SVR4.  */
#include <sys/mnttab.h>
#endif

#ifdef MOUNTED_VMOUNT           /* AIX.  */
#include <fshelp.h>
#include <sys/vfs.h>
#endif

#ifdef MOUNTED_INTERIX_STATVFS  /* Interix. */
#include <sys/statvfs.h>
#include <dirent.h>
#endif

#ifdef DOLPHIN
/* So special that it's not worth putting this in autoconf.  */
#undef MOUNTED_FREAD_FSTYP
#define MOUNTED_GETMNTTBL
#endif

#ifdef HAVE_SYS_MNTENT_H
/* This is to get MNTOPT_IGNORE on e.g. SVR4.  */
#include <sys/mntent.h>
#endif

#ifndef HAVE_HASMNTOPT
#define hasmntopt(mnt, opt) ((char *) 0)
#endif

#undef MNT_IGNORE
#ifdef MNTOPT_IGNORE
#define MNT_IGNORE(M) hasmntopt ((M), MNTOPT_IGNORE)
#else
#define MNT_IGNORE(M) 0
#endif

#ifdef HAVE_INFOMOUNT_QNX
#include <sys/disk.h>
#include <sys/fsys.h>
#endif

#ifdef HAVE_SYS_STATVFS_H       /* SVR4.  */
#include <sys/statvfs.h>
#endif

#include "lib/global.h"
#include "mountlist.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#if defined (__QNX__) && !defined(__QNXNTO__) && !defined (HAVE_INFOMOUNT_LIST)
#define HAVE_INFOMOUNT_QNX
#endif

#if defined(HAVE_INFOMOUNT_LIST) || defined(HAVE_INFOMOUNT_QNX)
#define HAVE_INFOMOUNT
#endif

/* The results of open() in this file are not used with fchdir,
   therefore save some unnecessary work in fchdir.c.  */
#undef open
#undef close

/* The results of opendir() in this file are not used with dirfd and fchdir,
   therefore save some unnecessary work in fchdir.c.  */
#undef opendir
#undef closedir

#define ME_DUMMY_0(Fs_name, Fs_type)            \
  (strcmp (Fs_type, "autofs") == 0              \
   || strcmp (Fs_type, "proc") == 0             \
   || strcmp (Fs_type, "subfs") == 0            \
   /* for Linux 2.6/3.x */                      \
   || strcmp (Fs_type, "debugfs") == 0          \
   || strcmp (Fs_type, "devpts") == 0           \
   || strcmp (Fs_type, "fusectl") == 0          \
   || strcmp (Fs_type, "mqueue") == 0           \
   || strcmp (Fs_type, "rpc_pipefs") == 0       \
   || strcmp (Fs_type, "sysfs") == 0            \
   /* FreeBSD, Linux 2.4 */                     \
   || strcmp (Fs_type, "devfs") == 0            \
   /* for NetBSD 3.0 */                         \
   || strcmp (Fs_type, "kernfs") == 0           \
   /* for Irix 6.5 */                           \
   || strcmp (Fs_type, "ignore") == 0)

/* Historically, we have marked as "dummy" any file system of type "none",
   but now that programs like du need to know about bind-mounted directories,
   we grant an exception to any with "bind" in its list of mount options.
   I.e., those are *not* dummy entries.  */
#ifdef MOUNTED_GETMNTENT1
#define ME_DUMMY(Fs_name, Fs_type, Fs_ent)      \
  (ME_DUMMY_0 (Fs_name, Fs_type)                \
   || (strcmp (Fs_type, "none") == 0            \
       && !hasmntopt (Fs_ent, "bind")))
#else
#define ME_DUMMY(Fs_name, Fs_type)              \
  (ME_DUMMY_0 (Fs_name, Fs_type) || strcmp (Fs_type, "none") == 0)
#endif

#ifdef __CYGWIN__
#include <windows.h>
#define ME_REMOTE me_remote
/* All cygwin mount points include `:' or start with `//'; so it
   requires a native Windows call to determine remote disks.  */
static int
me_remote (char const *fs_name, char const *fs_type _GL_UNUSED)
{
    if (fs_name[0] && fs_name[1] == ':')
    {
        char drive[4];
        sprintf (drive, "%c:\\", fs_name[0]);
        switch (GetDriveType (drive))
        {
        case DRIVE_REMOVABLE:
        case DRIVE_FIXED:
        case DRIVE_CDROM:
        case DRIVE_RAMDISK:
            return 0;
        }
    }
    return 1;
}
#endif
#ifndef ME_REMOTE
/* A file system is `remote' if its Fs_name contains a `:'
   or if (it is of type (smbfs or cifs) and its Fs_name starts with `//').  */
#define ME_REMOTE(Fs_name, Fs_type) \
    (strchr (Fs_name, ':') != NULL \
     || ((Fs_name)[0] == '/' \
         && (Fs_name)[1] == '/' \
         && (strcmp (Fs_type, "smbfs") == 0 || strcmp (Fs_type, "cifs") == 0)))
#endif

/* Many space usage primitives use all 1 bits to denote a value that is
   not applicable or unknown.  Propagate this information by returning
   a uintmax_t value that is all 1 bits if X is all 1 bits, even if X
   is unsigned and narrower than uintmax_t.  */
#define PROPAGATE_ALL_ONES(x) \
  ((sizeof (x) < sizeof (uintmax_t) \
    && (~ (x) == (sizeof (x) < sizeof (int) \
          ? - (1 << (sizeof (x) * CHAR_BIT)) \
          : 0))) \
   ? UINTMAX_MAX : (uintmax_t) (x))

/* Extract the top bit of X as an uintmax_t value.  */
#define EXTRACT_TOP_BIT(x) ((x) & ((uintmax_t) 1 << (sizeof (x) * CHAR_BIT - 1)))

/* If a value is negative, many space usage primitives store it into an
   integer variable by assignment, even if the variable's type is unsigned.
   So, if a space usage variable X's top bit is set, convert X to the
   uintmax_t value V such that (- (uintmax_t) V) is the negative of
   the original value.  If X's top bit is clear, just yield X.
   Use PROPAGATE_TOP_BIT if the original value might be negative;
   otherwise, use PROPAGATE_ALL_ONES.  */
#define PROPAGATE_TOP_BIT(x) ((x) | ~ (EXTRACT_TOP_BIT (x) - 1))

#ifdef STAT_STATVFS
/* Return true if statvfs works.  This is false for statvfs on systems
   with GNU libc on Linux kernels before 2.6.36, which stats all
   preceding entries in /proc/mounts; that makes df hang if even one
   of the corresponding file systems is hard-mounted but not available.  */
#if ! (__linux__ && (__GLIBC__ || __UCLIBC__))
/* The FRSIZE fallback is not required in this case.  */
#undef STAT_STATFS2_FRSIZE
static int
statvfs_works (void)
{
    return 1;
}
#else
#include <string.h>             /* for strverscmp */
#include <sys/utsname.h>
#include <sys/statfs.h>
#define STAT_STATFS2_BSIZE 1

static int
statvfs_works (void)
{
    static int statvfs_works_cache = -1;
    struct utsname name;

    if (statvfs_works_cache < 0)
        statvfs_works_cache = (uname (&name) == 0 && 0 <= strverscmp (name.release, "2.6.36"));
    return statvfs_works_cache;
}
#endif
#endif

#ifdef STAT_READ_FILSYS         /* SVR2 */
/* Set errno to zero upon EOF.  */
#define ZERO_BYTE_TRANSFER_ERRNO 0

#ifdef EINTR
#define IS_EINTR(x) ((x) == EINTR)
#else
#define IS_EINTR(x) 0
#endif
#endif /* STAT_READ_FILSYS */

/*** file scope type declarations ****************************************************************/

/* A mount table entry. */
struct mount_entry
{
    char *me_devname;           /* Device node name, including "/dev/". */
    char *me_mountdir;          /* Mount point directory name. */
    char *me_type;              /* "nfs", "4.2", etc. */
    dev_t me_dev;               /* Device number of me_mountdir. */
    unsigned int me_dummy:1;    /* Nonzero for dummy file systems. */
    unsigned int me_remote:1;   /* Nonzero for remote fileystems. */
    unsigned int me_type_malloced:1;    /* Nonzero if me_type was malloced. */
    struct mount_entry *me_next;
};

struct fs_usage
{
    uintmax_t fsu_blocksize;    /* Size of a block.  */
    uintmax_t fsu_blocks;       /* Total blocks. */
    uintmax_t fsu_bfree;        /* Free blocks available to superuser. */
    uintmax_t fsu_bavail;       /* Free blocks available to non-superuser. */
    int fsu_bavail_top_bit_set; /* 1 if fsu_bavail represents a value < 0.  */
    uintmax_t fsu_files;        /* Total file nodes. */
    uintmax_t fsu_ffree;        /* Free file nodes. */
};

/*** file scope variables ************************************************************************/

#ifdef HAVE_INFOMOUNT_LIST
static struct mount_entry *mc_mount_list = NULL;
#endif /* HAVE_INFOMOUNT_LIST */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_INFOMOUNT_LIST
static void
free_mount_entry (struct mount_entry *me)
{
    if (!me)
        return;
    g_free (me->me_devname);
    g_free (me->me_mountdir);
    if (me->me_type_malloced)
        g_free (me->me_type);
    g_free (me);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef MOUNTED_GETMNTINFO

#ifndef HAVE_STRUCT_STATFS_F_FSTYPENAME
static char *
fstype_to_string (short int t)
{
    switch (t)
    {
#ifdef MOUNT_PC
    case MOUNT_PC:
        return "pc";
#endif
#ifdef MOUNT_MFS
    case MOUNT_MFS:
        return "mfs";
#endif
#ifdef MOUNT_LO
    case MOUNT_LO:
        return "lo";
#endif
#ifdef MOUNT_TFS
    case MOUNT_TFS:
        return "tfs";
#endif
#ifdef MOUNT_TMP
    case MOUNT_TMP:
        return "tmp";
#endif
#ifdef MOUNT_UFS
    case MOUNT_UFS:
        return "ufs";
#endif
#ifdef MOUNT_NFS
    case MOUNT_NFS:
        return "nfs";
#endif
#ifdef MOUNT_MSDOS
    case MOUNT_MSDOS:
        return "msdos";
#endif
#ifdef MOUNT_LFS
    case MOUNT_LFS:
        return "lfs";
#endif
#ifdef MOUNT_LOFS
    case MOUNT_LOFS:
        return "lofs";
#endif
#ifdef MOUNT_FDESC
    case MOUNT_FDESC:
        return "fdesc";
#endif
#ifdef MOUNT_PORTAL
    case MOUNT_PORTAL:
        return "portal";
#endif
#ifdef MOUNT_NULL
    case MOUNT_NULL:
        return "null";
#endif
#ifdef MOUNT_UMAP
    case MOUNT_UMAP:
        return "umap";
#endif
#ifdef MOUNT_KERNFS
    case MOUNT_KERNFS:
        return "kernfs";
#endif
#ifdef MOUNT_PROCFS
    case MOUNT_PROCFS:
        return "procfs";
#endif
#ifdef MOUNT_AFS
    case MOUNT_AFS:
        return "afs";
#endif
#ifdef MOUNT_CD9660
    case MOUNT_CD9660:
        return "cd9660";
#endif
#ifdef MOUNT_UNION
    case MOUNT_UNION:
        return "union";
#endif
#ifdef MOUNT_DEVFS
    case MOUNT_DEVFS:
        return "devfs";
#endif
#ifdef MOUNT_EXT2FS
    case MOUNT_EXT2FS:
        return "ext2fs";
#endif
    default:
        return "?";
    }
}
#endif /* ! HAVE_STRUCT_STATFS_F_FSTYPENAME */

/* --------------------------------------------------------------------------------------------- */

static char *
fsp_to_string (const struct statfs *fsp)
{
#ifdef HAVE_STRUCT_STATFS_F_FSTYPENAME
    return (char *) (fsp->f_fstypename);
#else
    return fstype_to_string (fsp->f_type);
#endif
}
#endif /* MOUNTED_GETMNTINFO */

/* --------------------------------------------------------------------------------------------- */

#ifdef MOUNTED_VMOUNT           /* AIX.  */
static char *
fstype_to_string (int t)
{
    struct vfs_ent *e;

    e = getvfsbytype (t);
    if (!e || !e->vfsent_name)
        return "none";
    else
        return e->vfsent_name;
}
#endif /* MOUNTED_VMOUNT */

/* --------------------------------------------------------------------------------------------- */

#if defined MOUNTED_GETMNTENT1 || defined MOUNTED_GETMNTENT2

/* Return the device number from MOUNT_OPTIONS, if possible.
   Otherwise return (dev_t) -1.  */

/* --------------------------------------------------------------------------------------------- */

static dev_t
dev_from_mount_options (char const *mount_options)
{
    /* GNU/Linux allows file system implementations to define their own
       meaning for "dev=" mount options, so don't trust the meaning
       here.  */
#ifndef __linux__
    static char const dev_pattern[] = ",dev=";
    char const *devopt = strstr (mount_options, dev_pattern);

    if (devopt)
    {
        char const *optval = devopt + sizeof (dev_pattern) - 1;
        char *optvalend;
        unsigned long int dev;
        errno = 0;
        dev = strtoul (optval, &optvalend, 16);
        if (optval != optvalend
            && (*optvalend == '\0' || *optvalend == ',')
            && !(dev == ULONG_MAX && errno == ERANGE) && dev == (dev_t) dev)
            return dev;
    }
#endif

    (void) mount_options;
    return -1;
}

#endif

/* --------------------------------------------------------------------------------------------- */

#if defined _AIX && defined _I386
/* AIX PS/2 does not supply statfs.  */

static int
statfs (char *file, struct statfs *fsb)
{
    struct stat stats;
    struct dustat fsd;

    if (stat (file, &stats) != 0)
        return -1;
    if (dustat (stats.st_dev, 0, &fsd, sizeof (fsd)))
        return -1;
    fsb->f_type = 0;
    fsb->f_bsize = fsd.du_bsize;
    fsb->f_blocks = fsd.du_fsize - fsd.du_isize;
    fsb->f_bfree = fsd.du_tfree;
    fsb->f_bavail = fsd.du_tfree;
    fsb->f_files = (fsd.du_isize - 2) * fsd.du_inopb;
    fsb->f_ffree = fsd.du_tinode;
    fsb->f_fsid.val[0] = fsd.du_site;
    fsb->f_fsid.val[1] = fsd.du_pckno;
    return 0;
}

#endif /* _AIX && _I386 */

/* --------------------------------------------------------------------------------------------- */

/* Return a list of the currently mounted file systems, or NULL on error.
   Add each entry to the tail of the list so that they stay in order.
   If NEED_FS_TYPE is true, ensure that the file system type fields in
   the returned list are valid.  Otherwise, they might not be.  */

static struct mount_entry *
read_file_system_list (int need_fs_type)
{
    struct mount_entry *mount_list;
    struct mount_entry *me;
    struct mount_entry **mtail = &mount_list;

#ifdef MOUNTED_LISTMNTENT
    {
        struct tabmntent *mntlist, *p;
        struct mntent *mnt;
        struct mount_entry *me;

        /* the third and fourth arguments could be used to filter mounts,
           but Crays doesn't seem to have any mounts that we want to
           remove. Specifically, automount create normal NFS mounts.
         */

        if (listmntent (&mntlist, KMTAB, NULL, NULL) < 0)
            return NULL;
        for (p = mntlist; p; p = p->next)
        {
            mnt = p->ment;
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (mnt->mnt_fsname);
            me->me_mountdir = g_strdup (mnt->mnt_dir);
            me->me_type = g_strdup (mnt->mnt_type);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = -1;
            *mtail = me;
            mtail = &me->me_next;
        }
        freemntlist (mntlist);
    }
#endif

#ifdef MOUNTED_GETMNTENT1       /* GNU/Linux, 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
    {
        struct mntent *mnt;
        const char *table = MOUNTED;
        FILE *fp;

        fp = setmntent (table, "r");
        if (fp == NULL)
            return NULL;

        while ((mnt = getmntent (fp)))
        {
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (mnt->mnt_fsname);
            me->me_mountdir = g_strdup (mnt->mnt_dir);
            me->me_type = g_strdup (mnt->mnt_type);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type, mnt);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = dev_from_mount_options (mnt->mnt_opts);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }

        if (endmntent (fp) == 0)
            goto free_then_fail;
    }
#endif /* MOUNTED_GETMNTENT1. */

#ifdef MOUNTED_GETMNTINFO       /* 4.4BSD.  */
    {
        struct statfs *fsp;
        int entries;

        entries = getmntinfo (&fsp, MNT_NOWAIT);
        if (entries < 0)
            return NULL;
        for (; entries-- > 0; fsp++)
        {
            char *fs_type = fsp_to_string (fsp);

            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (fsp->f_mntfromname);
            me->me_mountdir = g_strdup (fsp->f_mntonname);
            me->me_type = fs_type;
            me->me_type_malloced = 0;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }
    }
#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD 3.0.  */
    {
        struct statvfs *fsp;
        int entries;

        entries = getmntinfo (&fsp, MNT_NOWAIT);
        if (entries < 0)
            return NULL;
        for (; entries-- > 0; fsp++)
        {
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (fsp->f_mntfromname);
            me->me_mountdir = g_strdup (fsp->f_mntonname);
            me->me_type = g_strdup (fsp->f_fstypename);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }
    }
#endif /* MOUNTED_GETMNTINFO2 */

#ifdef MOUNTED_GETMNT           /* Ultrix.  */
    {
        int offset = 0;
        int val;
        struct fs_data fsd;

        while (errno = 0, 0 < (val = getmnt (&offset, &fsd, sizeof (fsd), NOSTAT_MANY, (char *) 0)))
        {
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (fsd.fd_req.devname);
            me->me_mountdir = g_strdup (fsd.fd_req.path);
            me->me_type = gt_names[fsd.fd_req.fstype];
            me->me_type_malloced = 0;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = fsd.fd_req.dev;

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }
        if (val < 0)
            goto free_then_fail;
    }
#endif /* MOUNTED_GETMNT. */

#if defined MOUNTED_FS_STAT_DEV /* BeOS */
    {
        /* The next_dev() and fs_stat_dev() system calls give the list of
           all file systems, including the information returned by statvfs()
           (fs type, total blocks, free blocks etc.), but without the mount
           point. But on BeOS all file systems except / are mounted in the
           rootfs, directly under /.
           The directory name of the mount point is often, but not always,
           identical to the volume name of the device.
           We therefore get the list of subdirectories of /, and the list
           of all file systems, and match the two lists.  */

        DIR *dirp;
        struct rootdir_entry
        {
            char *name;
            dev_t dev;
            ino_t ino;
            struct rootdir_entry *next;
        };
        struct rootdir_entry *rootdir_list;
        struct rootdir_entry **rootdir_tail;
        int32 pos;
        dev_t dev;
        fs_info fi;

        /* All volumes are mounted in the rootfs, directly under /. */
        rootdir_list = NULL;
        rootdir_tail = &rootdir_list;
        dirp = opendir ("/");
        if (dirp)
        {
            struct dirent *d;

            while ((d = readdir (dirp)) != NULL)
            {
                char *name;
                struct stat statbuf;

                if (strcmp (d->d_name, "..") == 0)
                    continue;

                if (strcmp (d->d_name, ".") == 0)
                    name = g_strdup ("/");
                else
                    name = g_strconcat ("/", d->d_name, (char *) NULL);

                if (lstat (name, &statbuf) >= 0 && S_ISDIR (statbuf.st_mode))
                {
                    struct rootdir_entry *re = g_malloc (sizeof (*re));
                    re->name = name;
                    re->dev = statbuf.st_dev;
                    re->ino = statbuf.st_ino;

                    /* Add to the linked list.  */
                    *rootdir_tail = re;
                    rootdir_tail = &re->next;
                }
                else
                    g_free (name);
            }
            closedir (dirp);
        }
        *rootdir_tail = NULL;

        for (pos = 0; (dev = next_dev (&pos)) >= 0;)
            if (fs_stat_dev (dev, &fi) >= 0)
            {
                /* Note: fi.dev == dev. */
                struct rootdir_entry *re;

                for (re = rootdir_list; re; re = re->next)
                    if (re->dev == fi.dev && re->ino == fi.root)
                        break;

                me = g_malloc (sizeof (*me));
                me->me_devname =
                    g_strdup (fi.device_name[0] != '\0' ? fi.device_name : fi.fsh_name);
                me->me_mountdir = g_strdup (re != NULL ? re->name : fi.fsh_name);
                me->me_type = g_strdup (fi.fsh_name);
                me->me_type_malloced = 1;
                me->me_dev = fi.dev;
                me->me_dummy = 0;
                me->me_remote = (fi.flags & B_FS_IS_SHARED) != 0;

                /* Add to the linked list. */
                *mtail = me;
                mtail = &me->me_next;
            }
        *mtail = NULL;

        while (rootdir_list != NULL)
        {
            struct rootdir_entry *re = rootdir_list;
            rootdir_list = re->next;
            g_free (re->name);
            g_free (re);
        }
    }
#endif /* MOUNTED_FS_STAT_DEV */

#ifdef MOUNTED_GETFSSTAT        /* __alpha running OSF_1 */
    {
        int numsys, counter;
        size_t bufsize;
        struct statfs *stats;

        numsys = getfsstat (NULL, 0L, MNT_NOWAIT);
        if (numsys < 0)
            return NULL;
        if (SIZE_MAX / sizeof (*stats) <= numsys)
        {
            fprintf (stderr, "%s\n", _("Memory exhausted!"));
            exit (EXIT_FAILURE);
        }

        bufsize = (1 + numsys) * sizeof (*stats);
        stats = g_malloc (bufsize);
        numsys = getfsstat (stats, bufsize, MNT_NOWAIT);

        if (numsys < 0)
        {
            g_free (stats);
            return NULL;
        }

        for (counter = 0; counter < numsys; counter++)
        {
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup (stats[counter].f_mntfromname);
            me->me_mountdir = g_strdup (stats[counter].f_mntonname);
            me->me_type = g_strdup (FS_TYPE (stats[counter]));
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }

        g_free (stats);
    }
#endif /* MOUNTED_GETFSSTAT */

#if defined MOUNTED_FREAD || defined MOUNTED_FREAD_FSTYP        /* SVR[23].  */
    {
        struct mnttab mnt;
        char *table = "/etc/mnttab";
        FILE *fp;

        fp = fopen (table, "r");
        if (fp == NULL)
            return NULL;

        while (fread (&mnt, sizeof (mnt), 1, fp) > 0)
        {
            me = g_malloc (sizeof (*me));
#ifdef GETFSTYP                 /* SVR3.  */
            me->me_devname = g_strdup (mnt.mt_dev);
#else
            me->me_devname = g_strconcat ("/dev/", mnt.mt_dev, (char *) NULL);
#endif
            me->me_mountdir = g_strdup (mnt.mt_filsys);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */
            me->me_type = "";
            me->me_type_malloced = 0;
#ifdef GETFSTYP                 /* SVR3.  */
            if (need_fs_type)
            {
                struct statfs fsd;
                char typebuf[FSTYPSZ];

                if (statfs (me->me_mountdir, &fsd, sizeof (fsd), 0) != -1
                    && sysfs (GETFSTYP, fsd.f_fstyp, typebuf) != -1)
                {
                    me->me_type = g_strdup (typebuf);
                    me->me_type_malloced = 1;
                }
            }
#endif
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }

        if (ferror (fp))
        {
            /* The last fread() call must have failed.  */
            int saved_errno = errno;
            fclose (fp);
            errno = saved_errno;
            goto free_then_fail;
        }

        if (fclose (fp) == EOF)
            goto free_then_fail;
    }
#endif /* MOUNTED_FREAD || MOUNTED_FREAD_FSTYP.  */

#ifdef MOUNTED_GETMNTTBL        /* DolphinOS goes its own way.  */
    {
        struct mntent **mnttbl = getmnttbl (), **ent;
        for (ent = mnttbl; *ent; ent++)
        {
            me = g_malloc (sizeof (*me));
            me->me_devname = g_strdup ((*ent)->mt_resource);
            me->me_mountdir = g_strdup ((*ent)->mt_directory);
            me->me_type = g_strdup ((*ent)->mt_fstype);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }
        endmnttbl ();
    }
#endif

#ifdef MOUNTED_GETMNTENT2       /* SVR4.  */
    {
        struct mnttab mnt;
        char *table = MNTTAB;
        FILE *fp;
        int ret;
        int lockfd = -1;

#if defined F_RDLCK && defined F_SETLKW
        /* MNTTAB_LOCK is a macro name of our own invention; it's not present in
           e.g. Solaris 2.6.  If the SVR4 folks ever define a macro
           for this file name, we should use their macro name instead.
           (Why not just lock MNTTAB directly?  We don't know.)  */
#ifndef MNTTAB_LOCK
#define MNTTAB_LOCK "/etc/.mnttab.lock"
#endif
        lockfd = open (MNTTAB_LOCK, O_RDONLY);
        if (0 <= lockfd)
        {
            struct flock flock;
            flock.l_type = F_RDLCK;
            flock.l_whence = SEEK_SET;
            flock.l_start = 0;
            flock.l_len = 0;
            while (fcntl (lockfd, F_SETLKW, &flock) == -1)
                if (errno != EINTR)
                {
                    int saved_errno = errno;
                    close (lockfd);
                    errno = saved_errno;
                    return NULL;
                }
        }
        else if (errno != ENOENT)
            return NULL;
#endif

        errno = 0;
        fp = fopen (table, "r");
        if (fp == NULL)
            ret = errno;
        else
        {
            while ((ret = getmntent (fp, &mnt)) == 0)
            {
                me = g_malloc (sizeof (*me));
                me->me_devname = g_strdup (mnt.mnt_special);
                me->me_mountdir = g_strdup (mnt.mnt_mountp);
                me->me_type = g_strdup (mnt.mnt_fstype);
                me->me_type_malloced = 1;
                me->me_dummy = MNT_IGNORE (&mnt) != 0;
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = dev_from_mount_options (mnt.mnt_mntopts);

                /* Add to the linked list. */
                *mtail = me;
                mtail = &me->me_next;
            }

            ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
        }

        if (0 <= lockfd && close (lockfd) != 0)
            ret = errno;

        if (0 <= ret)
        {
            errno = ret;
            goto free_then_fail;
        }
    }
#endif /* MOUNTED_GETMNTENT2.  */

#ifdef MOUNTED_VMOUNT           /* AIX.  */
    {
        int bufsize;
        char *entries, *thisent;
        struct vmount *vmp;
        int n_entries;
        int i;

        /* Ask how many bytes to allocate for the mounted file system info.  */
        if (mntctl (MCTL_QUERY, sizeof (bufsize), (struct vmount *) &bufsize) != 0)
            return NULL;
        entries = g_malloc (bufsize);

        /* Get the list of mounted file systems.  */
        n_entries = mntctl (MCTL_QUERY, bufsize, (struct vmount *) entries);
        if (n_entries < 0)
        {
            int saved_errno = errno;
            g_free (entries);
            errno = saved_errno;
            return NULL;
        }

        for (i = 0, thisent = entries; i < n_entries; i++, thisent += vmp->vmt_length)
        {
            char *options, *ignore;

            vmp = (struct vmount *) thisent;
            me = g_malloc (sizeof (*me));
            if (vmp->vmt_flags & MNT_REMOTE)
            {
                char *host, *dir;

                me->me_remote = 1;
                /* Prepend the remote dirname.  */
                host = thisent + vmp->vmt_data[VMT_HOSTNAME].vmt_off;
                dir = thisent + vmp->vmt_data[VMT_OBJECT].vmt_off;
                me->me_devname = g_strconcat (host, ":", dir, (char *) NULL);
            }
            else
            {
                me->me_remote = 0;
                me->me_devname = g_strdup (thisent + vmp->vmt_data[VMT_OBJECT].vmt_off);
            }
            me->me_mountdir = g_strdup (thisent + vmp->vmt_data[VMT_STUB].vmt_off);
            me->me_type = g_strdup (fstype_to_string (vmp->vmt_gfstype));
            me->me_type_malloced = 1;
            options = thisent + vmp->vmt_data[VMT_ARGS].vmt_off;
            ignore = strstr (options, "ignore");
            me->me_dummy = (ignore
                            && (ignore == options || ignore[-1] == ',')
                            && (ignore[sizeof ("ignore") - 1] == ','
                                || ignore[sizeof ("ignore") - 1] == '\0'));
            me->me_dev = (dev_t) (-1);  /* vmt_fsid might be the info we want.  */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
        }
        g_free (entries);
    }
#endif /* MOUNTED_VMOUNT. */


#ifdef MOUNTED_INTERIX_STATVFS
    {
        DIR *dirp = opendir ("/dev/fs");
        char node[9 + NAME_MAX];

        if (!dirp)
            goto free_then_fail;

        while (1)
        {
            struct statvfs dev;
            struct dirent entry;
            struct dirent *result;

            if (readdir_r (dirp, &entry, &result) || result == NULL)
                break;

            strcpy (node, "/dev/fs/");
            strcat (node, entry.d_name);

            if (statvfs (node, &dev) == 0)
            {
                me = g_malloc (sizeof *me);
                me->me_devname = g_strdup (dev.f_mntfromname);
                me->me_mountdir = g_strdup (dev.f_mntonname);
                me->me_type = g_strdup (dev.f_fstypename);
                me->me_type_malloced = 1;
                me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = (dev_t) (-1);      /* Magic; means not known yet. */

                /* Add to the linked list. */
                *mtail = me;
                mtail = &me->me_next;
            }
        }
    }
#endif /* MOUNTED_INTERIX_STATVFS */

    (void) need_fs_type;        /* avoid argument-unused warning */
    *mtail = NULL;
    return mount_list;


  free_then_fail:
    {
        int saved_errno = errno;
        *mtail = NULL;

        while (mount_list)
        {
            me = mount_list->me_next;
            g_free (mount_list->me_devname);
            g_free (mount_list->me_mountdir);
            if (mount_list->me_type_malloced)
                g_free (mount_list->me_type);
            g_free (mount_list);
            mount_list = me;
        }

        errno = saved_errno;
        return NULL;
    }
}

#endif /* HAVE_INFOMOUNT_LIST */

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_INFOMOUNT_QNX
/**
 ** QNX has no [gs]etmnt*(), [gs]etfs*(), or /etc/mnttab, but can do
 ** this via the following code.
 ** Note that, as this is based on CWD, it only fills one mount_entry
 ** structure. See my_statfs() in utilunix.c for the "other side" of
 ** this hack.
 */

static struct mount_entry *
read_file_system_list (int need_fs_type, int all_fs)
{
    struct _disk_entry de;
    struct statfs fs;
    int i, fd;
    char *tp, dev[_POSIX_NAME_MAX], dir[_POSIX_PATH_MAX];

    static struct mount_entry *me = NULL;

    if (me)
    {
        g_free (me->me_devname);
        g_free (me->me_mountdir);
        g_free (me->me_type);
    }
    else
        me = (struct mount_entry *) g_malloc (sizeof (struct mount_entry));

    if (!getcwd (dir, _POSIX_PATH_MAX))
        return (NULL);

    fd = open (dir, O_RDONLY);
    if (fd == -1)
        return (NULL);

    i = disk_get_entry (fd, &de);

    close (fd);

    if (i == -1)
        return (NULL);

    switch (de.disk_type)
    {
    case _UNMOUNTED:
        tp = "unmounted";
        break;
    case _FLOPPY:
        tp = "Floppy";
        break;
    case _HARD:
        tp = "Hard";
        break;
    case _RAMDISK:
        tp = "Ram";
        break;
    case _REMOVABLE:
        tp = "Removable";
        break;
    case _TAPE:
        tp = "Tape";
        break;
    case _CDROM:
        tp = "CDROM";
        break;
    default:
        tp = "unknown";
    }

    if (fsys_get_mount_dev (dir, &dev) == -1)
        return (NULL);

    if (fsys_get_mount_pt (dev, &dir) == -1)
        return (NULL);

    me->me_devname = g_strdup (dev);
    me->me_mountdir = g_strdup (dir);
    me->me_type = g_strdup (tp);
    me->me_dev = de.disk_type;

#ifdef DEBUG
    fprintf (stderr,
             "disk_get_entry():\n\tdisk_type=%d (%s)\n\tdriver_name='%-*.*s'\n\tdisk_drv=%d\n",
             de.disk_type, tp, _DRIVER_NAME_LEN, _DRIVER_NAME_LEN, de.driver_name, de.disk_drv);
    fprintf (stderr, "fsys_get_mount_dev():\n\tdevice='%s'\n", dev);
    fprintf (stderr, "fsys_get_mount_pt():\n\tmount point='%s'\n", dir);
#endif /* DEBUG */

    return (me);
}
#endif /* HAVE_INFOMOUNT_QNX */

/* --------------------------------------------------------------------------------------------- */

#ifdef STAT_READ_FILSYS         /* SVR2 */

/* Read(write) up to COUNT bytes at BUF from(to) descriptor FD, retrying if
   interrupted.  Return the actual number of bytes read(written), zero for EOF,
   or SAFE_READ_ERROR(SAFE_WRITE_ERROR) upon error.  */
static size_t
safe_read (int fd, void *buf, size_t count)
{
    /* Work around a bug in Tru64 5.1.  Attempting to read more than
       INT_MAX bytes fails with errno == EINVAL.  See
       <http://lists.gnu.org/archive/html/bug-gnu-utils/2002-04/msg00010.html>.
       When decreasing COUNT, keep it block-aligned.  */
    /* *INDENT-OFF* */
    enum { BUGGY_READ_MAXIMUM = INT_MAX & ~8191 };
    /* *INDENT-ON* */

    while (TRUE)
    {
        ssize_t result;

        result = read (fd, buf, count);

        if (0 <= result)
            return result;
        else if (IS_EINTR (errno))
            continue;
        else if (errno == EINVAL && BUGGY_READ_MAXIMUM < count)
            count = BUGGY_READ_MAXIMUM;
        else
            return result;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Read COUNT bytes at BUF to(from) descriptor FD, retrying if
   interrupted or if a partial write(read) occurs.  Return the number
   of bytes transferred.
   When writing, set errno if fewer than COUNT bytes are written.
   When reading, if fewer than COUNT bytes are read, you must examine
   errno to distinguish failure from EOF (errno == 0).  */

static size_t
full_read (int fd, void *buf, size_t count)
{
    size_t total = 0;
    char *ptr = (char *) buf;

    while (count > 0)
    {
        size_t n_rw = safe_read (fd, ptr, count);
        if (n_rw == (size_t) (-1))
            break;
        if (n_rw == 0)
        {
            errno = ZERO_BYTE_TRANSFER_ERRNO;
            break;
        }
        total += n_rw;
        ptr += n_rw;
        count -= n_rw;
    }

    return total;
}

#endif /* STAT_READ_FILSYS */

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_INFOMOUNT
/* Fill in the fields of FSP with information about space usage for
   the file system on which FILE resides.
   DISK is the device on which FILE is mounted, for space-getting
   methods that need to know it.
   Return 0 if successful, -1 if not.  When returning -1, ensure that
   ERRNO is either a system error value, or zero if DISK is NULL
   on a system that requires a non-NULL value.  */
static int
get_fs_usage (char const *file, char const *disk, struct fs_usage *fsp)
{
#ifdef STAT_STATVFS             /* POSIX, except pre-2.6.36 glibc/Linux */

    if (statvfs_works ())
    {
        struct statvfs vfsd;

        if (statvfs (file, &vfsd) < 0)
            return -1;

        /* f_frsize isn't guaranteed to be supported.  */
        fsp->fsu_blocksize = (vfsd.f_frsize
                              ? PROPAGATE_ALL_ONES (vfsd.f_frsize)
                              : PROPAGATE_ALL_ONES (vfsd.f_bsize));

        fsp->fsu_blocks = PROPAGATE_ALL_ONES (vfsd.f_blocks);
        fsp->fsu_bfree = PROPAGATE_ALL_ONES (vfsd.f_bfree);
        fsp->fsu_bavail = PROPAGATE_TOP_BIT (vfsd.f_bavail);
        fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (vfsd.f_bavail) != 0;
        fsp->fsu_files = PROPAGATE_ALL_ONES (vfsd.f_files);
        fsp->fsu_ffree = PROPAGATE_ALL_ONES (vfsd.f_ffree);
    }
    else
#endif

    {
#if defined STAT_STATVFS64      /* AIX */

        struct statvfs64 fsd;

        if (statvfs64 (file, &fsd) < 0)
            return -1;

        /* f_frsize isn't guaranteed to be supported.  */
        /* *INDENT-OFF* */
        fsp->fsu_blocksize = fsd.f_frsize
            ? PROPAGATE_ALL_ONES (fsd.f_frsize)
            : PROPAGATE_ALL_ONES (fsd.f_bsize);
        /* *INDENT-ON* */

#elif defined STAT_STATFS2_FS_DATA      /* Ultrix */

        struct fs_data fsd;

        if (statfs (file, &fsd) != 1)
            return -1;

        fsp->fsu_blocksize = 1024;
        fsp->fsu_blocks = PROPAGATE_ALL_ONES (fsd.fd_req.btot);
        fsp->fsu_bfree = PROPAGATE_ALL_ONES (fsd.fd_req.bfree);
        fsp->fsu_bavail = PROPAGATE_TOP_BIT (fsd.fd_req.bfreen);
        fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (fsd.fd_req.bfreen) != 0;
        fsp->fsu_files = PROPAGATE_ALL_ONES (fsd.fd_req.gtot);
        fsp->fsu_ffree = PROPAGATE_ALL_ONES (fsd.fd_req.gfree);

#elif defined STAT_READ_FILSYS  /* SVR2 */
#ifndef SUPERBOFF
#define SUPERBOFF (SUPERB * 512)
#endif

        struct filsys fsd;
        int fd;

        if (!disk)
        {
            errno = 0;
            return -1;
        }

        fd = open (disk, O_RDONLY);
        if (fd < 0)
            return -1;
        lseek (fd, (off_t) SUPERBOFF, 0);
        if (full_read (fd, (char *) &fsd, sizeof (fsd)) != sizeof (fsd))
        {
            close (fd);
            return -1;
        }
        close (fd);

        fsp->fsu_blocksize = (fsd.s_type == Fs2b ? 1024 : 512);
        fsp->fsu_blocks = PROPAGATE_ALL_ONES (fsd.s_fsize);
        fsp->fsu_bfree = PROPAGATE_ALL_ONES (fsd.s_tfree);
        fsp->fsu_bavail = PROPAGATE_TOP_BIT (fsd.s_tfree);
        fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (fsd.s_tfree) != 0;
        fsp->fsu_files = (fsd.s_isize == -1
                          ? UINTMAX_MAX : (fsd.s_isize - 2) * INOPB * (fsd.s_type == Fs2b ? 2 : 1));
        fsp->fsu_ffree = PROPAGATE_ALL_ONES (fsd.s_tinode);

#elif defined STAT_STATFS3_OSF1 /* OSF/1 */

        struct statfs fsd;

        if (statfs (file, &fsd, sizeof (struct statfs)) != 0)
            return -1;

        fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_fsize);

#elif defined STAT_STATFS2_FRSIZE       /* 2.6 < glibc/Linux < 2.6.36 */

        struct statfs fsd;

        if (statfs (file, &fsd) < 0)
            return -1;

        fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_frsize);

#elif defined STAT_STATFS2_BSIZE        /* glibc/Linux < 2.6, 4.3BSD, SunOS 4, \
                                           Mac OS X < 10.4, FreeBSD < 5.0, \
                                           NetBSD < 3.0, OpenBSD < 4.4 */

        struct statfs fsd;

        if (statfs (file, &fsd) < 0)
            return -1;

        fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_bsize);

#ifdef STATFS_TRUNCATES_BLOCK_COUNTS

        /* In SunOS 4.1.2, 4.1.3, and 4.1.3_U1, the block counts in the
           struct statfs are truncated to 2GB.  These conditions detect that
           truncation, presumably without botching the 4.1.1 case, in which
           the values are not truncated.  The correct counts are stored in
           undocumented spare fields.  */
        if (fsd.f_blocks == 0x7fffffff / fsd.f_bsize && fsd.f_spare[0] > 0)
        {
            fsd.f_blocks = fsd.f_spare[0];
            fsd.f_bfree = fsd.f_spare[1];
            fsd.f_bavail = fsd.f_spare[2];
        }
#endif /* STATFS_TRUNCATES_BLOCK_COUNTS */

#elif defined STAT_STATFS2_FSIZE        /* 4.4BSD and older NetBSD */

        struct statfs fsd;

        if (statfs (file, &fsd) < 0)
            return -1;

        fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_fsize);

#elif defined STAT_STATFS4      /* SVR3, Dynix, old Irix, old AIX, \
                                   Dolphin */

#if !defined _AIX && !defined _SEQUENT_ && !defined DOLPHIN
#define f_bavail f_bfree
#endif

        struct statfs fsd;

        if (statfs (file, &fsd, sizeof (fsd), 0) < 0)
            return -1;

        /* Empirically, the block counts on most SVR3 and SVR3-derived
           systems seem to always be in terms of 512-byte blocks,
           no matter what value f_bsize has.  */
#if defined _AIX || defined _CRAY
        fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_bsize);
#else
        fsp->fsu_blocksize = 512;
#endif

#endif

#if (defined STAT_STATVFS64 || defined STAT_STATFS3_OSF1 \
     || defined STAT_STATFS2_FRSIZE || defined STAT_STATFS2_BSIZE \
     || defined STAT_STATFS2_FSIZE || defined STAT_STATFS4)

        fsp->fsu_blocks = PROPAGATE_ALL_ONES (fsd.f_blocks);
        fsp->fsu_bfree = PROPAGATE_ALL_ONES (fsd.f_bfree);
        fsp->fsu_bavail = PROPAGATE_TOP_BIT (fsd.f_bavail);
        fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (fsd.f_bavail) != 0;
        fsp->fsu_files = PROPAGATE_ALL_ONES (fsd.f_files);
        fsp->fsu_ffree = PROPAGATE_ALL_ONES (fsd.f_ffree);

#endif
    }

    (void) disk;                /* avoid argument-unused warning */

    return 0;
}
#endif /* HAVE_INFOMOUNT */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
free_my_statfs (void)
{
#ifdef HAVE_INFOMOUNT_LIST
    while (mc_mount_list != NULL)
    {
        struct mount_entry *next;

        next = mc_mount_list->me_next;
        free_mount_entry (mc_mount_list);
        mc_mount_list = next;
    }

    mc_mount_list = NULL;
#endif /* HAVE_INFOMOUNT_LIST */
}

/* --------------------------------------------------------------------------------------------- */

void
init_my_statfs (void)
{
#ifdef HAVE_INFOMOUNT_LIST
    free_my_statfs ();
    mc_mount_list = read_file_system_list (1);
#endif /* HAVE_INFOMOUNT_LIST */
}

/* --------------------------------------------------------------------------------------------- */

void
my_statfs (struct my_statfs *myfs_stats, const char *path)
{
#ifdef HAVE_INFOMOUNT_LIST
    size_t i, len = 0;
    struct mount_entry *entry = NULL;
    struct mount_entry *temp = mc_mount_list;
    struct fs_usage fs_use;

    while (temp)
    {
        i = strlen (temp->me_mountdir);
        if (i > len && (strncmp (path, temp->me_mountdir, i) == 0))
            if (!entry || (path[i] == PATH_SEP || path[i] == '\0'))
            {
                len = i;
                entry = temp;
            }
        temp = temp->me_next;
    }

    if (entry)
    {
        memset (&fs_use, 0, sizeof (struct fs_usage));
        get_fs_usage (entry->me_mountdir, NULL, &fs_use);

        myfs_stats->type = entry->me_dev;
        myfs_stats->typename = entry->me_type;
        myfs_stats->mpoint = entry->me_mountdir;
        myfs_stats->device = entry->me_devname;
        myfs_stats->avail =
            ((uintmax_t) (getuid ()? fs_use.fsu_bavail : fs_use.fsu_bfree) *
             fs_use.fsu_blocksize) >> 10;
        myfs_stats->total = ((uintmax_t) fs_use.fsu_blocks * fs_use.fsu_blocksize) >> 10;
        myfs_stats->nfree = (uintmax_t) fs_use.fsu_ffree;
        myfs_stats->nodes = (uintmax_t) fs_use.fsu_files;
    }
    else
#endif /* HAVE_INFOMOUNT_LIST */

#ifdef HAVE_INFOMOUNT_QNX
        /*
         ** This is the "other side" of the hack to read_file_system_list() in
         ** mountlist.c.
         ** It's not the most efficient approach, but consumes less memory. It
         ** also accomodates QNX's ability to mount filesystems on the fly.
         */
        struct mount_entry *entry;
    struct fs_usage fs_use;

    entry = read_file_system_list (0, 0);
    if (entry != NULL)
    {
        get_fs_usage (entry->me_mountdir, NULL, &fs_use);

        myfs_stats->type = entry->me_dev;
        myfs_stats->typename = entry->me_type;
        myfs_stats->mpoint = entry->me_mountdir;
        myfs_stats->device = entry->me_devname;

        myfs_stats->avail = ((uintmax_t) fs_use.fsu_bfree * fs_use.fsu_blocksize) >> 10;
        myfs_stats->total = ((uintmax_t) fs_use.fsu_blocks * fs_use.fsu_blocksize) >> 10;
        myfs_stats->nfree = (uintmax_t) fs_use.fsu_ffree;
        myfs_stats->nodes = (uintmax_t) fs_use.fsu_files;
    }
    else
#endif /* HAVE_INFOMOUNT_QNX */
    {
        myfs_stats->type = 0;
        myfs_stats->mpoint = "unknown";
        myfs_stats->device = "unknown";
        myfs_stats->avail = 0;
        myfs_stats->total = 0;
        myfs_stats->nfree = 0;
        myfs_stats->nodes = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */
