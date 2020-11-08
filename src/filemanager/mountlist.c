/*
   Return a list of mounted file systems

   Copyright (C) 1991-2020
   Free Software Foundation, Inc.

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

#ifdef MOUNTED_GETFSSTAT        /* OSF_1, also (obsolete) Apple Darwin 1.3 */
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
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef MOUNTED_GETMNTENT1       /* glibc, HP-UX, IRIX, Cygwin, Android,
                                   also (obsolete) 4.3BSD, SunOS */
#include <mntent.h>
#include <sys/types.h>
#if defined __ANDROID__         /* Android */
   /* Bionic versions from between 2014-01-09 and 2015-01-08 define MOUNTED to
      an incorrect value; older Bionic versions don't define it at all.  */
#undef MOUNTED
#define MOUNTED "/proc/mounts"
#elif !defined  MOUNTED
#ifdef _PATH_MOUNTED            /* GNU libc  */
#define MOUNTED _PATH_MOUNTED
#endif
#ifdef MNT_MNTTAB               /* HP-UX.  */
#define MOUNTED MNT_MNTTAB
#endif
#endif
#endif

#ifdef MOUNTED_GETMNTINFO       /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD  */
#include <sys/mount.h>
#endif

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD, Minix */
#include <sys/statvfs.h>
#endif

#ifdef MOUNTED_FS_STAT_DEV      /* Haiku, also (obsolete) BeOS */
#include <fs_info.h>
#include <dirent.h>
#endif

#ifdef MOUNTED_FREAD_FSTYP      /* (obsolete) SVR3 */
#include <mnttab.h>
#include <sys/fstyp.h>
#include <sys/statfs.h>
#endif

#ifdef MOUNTED_GETEXTMNTENT     /* Solaris >= 8 */
#include <sys/mnttab.h>
#endif

#ifdef MOUNTED_GETMNTENT2       /* Solaris < 8, also (obsolete) SVR4 */
#include <sys/mnttab.h>
#endif

#ifdef MOUNTED_VMOUNT           /* AIX */
#include <fshelp.h>
#include <sys/vfs.h>
#endif

#ifdef MOUNTED_INTERIX_STATVFS  /* Interix */
#include <sys/statvfs.h>
#include <dirent.h>
#endif

#ifdef HAVE_SYS_MNTENT_H
/* This is to get MNTOPT_IGNORE on e.g. SVR4.  */
#include <sys/mntent.h>
#endif

#ifdef MOUNTED_GETMNTENT1
#if !HAVE_SETMNTENT             /* Android <= 4.4 */
#define setmntent(fp,mode) fopen (fp, mode)
#endif
#if !HAVE_ENDMNTENT             /* Android <= 4.4 */
#define endmntent(fp) fclose (fp)
#endif
#endif

#ifndef HAVE_HASMNTOPT
#define hasmntopt(mnt, opt) ((char *) 0)
#endif

#undef MNT_IGNORE
#ifdef MNTOPT_IGNORE
#if defined __sun && defined __SVR4
/* Solaris defines hasmntopt(struct mnttab *, char *)
   while it is otherwise hasmntopt(struct mnttab *, const char *).  */
#define MNT_IGNORE(M) hasmntopt (M, (char *) MNTOPT_IGNORE)
#else
#define MNT_IGNORE(M) hasmntopt (M, MNTOPT_IGNORE)
#endif
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
#include "lib/strutil.h"        /* str_verscmp() */
#include "lib/unixcompat.h"     /* makedev */
#include "mountlist.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#if defined (__QNX__) && !defined(__QNXNTO__) && !defined (HAVE_INFOMOUNT_LIST)
#define HAVE_INFOMOUNT_QNX
#endif

#if defined(HAVE_INFOMOUNT_LIST) || defined(HAVE_INFOMOUNT_QNX)
#define HAVE_INFOMOUNT
#endif

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
#define ME_DUMMY(Fs_name, Fs_type, Bind)        \
  (ME_DUMMY_0 (Fs_name, Fs_type)                \
   || (strcmp (Fs_type, "none") == 0 && !Bind))
#else
#define ME_DUMMY(Fs_name, Fs_type)              \
  (ME_DUMMY_0 (Fs_name, Fs_type) || strcmp (Fs_type, "none") == 0)
#endif

#ifdef __CYGWIN__
#include <windows.h>
#define ME_REMOTE me_remote
/* All cygwin mount points include ':' or start with '//'; so it
   requires a native Windows call to determine remote disks.  */
static int
me_remote (char const *fs_name, char const *fs_type)
{
    (void) fs_type;

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
/* A file system is 'remote' if its Fs_name contains a ':'
   or if (it is of type (smbfs or smb3 or cifs) and its Fs_name starts with '//')
   or if it is of any other of the listed types
   or Fs_name is equal to "-hosts" (used by autofs to mount remote fs).
   "VM" file systems like prl_fs or vboxsf are not considered remote here. */
#define ME_REMOTE(Fs_name, Fs_type) \
    (strchr (Fs_name, ':') != NULL \
     || ((Fs_name)[0] == '/' \
         && (Fs_name)[1] == '/' \
         && (strcmp (Fs_type, "smbfs") == 0 \
             || strcmp (Fs_type, "smb3") == 0 \
             || strcmp (Fs_type, "cifs") == 0)) \
     || strcmp (Fs_type, "acfs") == 0 \
     || strcmp (Fs_type, "afs") == 0 \
     || strcmp (Fs_type, "coda") == 0 \
     || strcmp (Fs_type, "auristorfs") == 0 \
     || strcmp (Fs_type, "fhgfs") == 0 \
     || strcmp (Fs_type, "gpfs") == 0 \
     || strcmp (Fs_type, "ibrix") == 0 \
     || strcmp (Fs_type, "ocfs2") == 0 \
     || strcmp (Fs_type, "vxfs") == 0 \
     || strcmp ("-hosts", Fs_name) == 0)
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
#if ! (defined __linux__ && (defined __GLIBC__ || defined __UCLIBC__))
/* The FRSIZE fallback is not required in this case.  */
#undef STAT_STATFS2_FRSIZE
#else
#include <sys/utsname.h>
#include <sys/statfs.h>
#define STAT_STATFS2_BSIZE 1
#endif
#endif

/*** file scope type declarations ****************************************************************/

/* A mount table entry. */
struct mount_entry
{
    char *me_devname;           /* Device node name, including "/dev/". */
    char *me_mountdir;          /* Mount point directory name. */
    char *me_mntroot;           /* Directory on filesystem of device used
                                   as root for the (bind) mount. */
    char *me_type;              /* "nfs", "4.2", etc. */
    dev_t me_dev;               /* Device number of me_mountdir. */
    unsigned int me_dummy:1;    /* Nonzero for dummy file systems. */
    unsigned int me_remote:1;   /* Nonzero for remote fileystems. */
    unsigned int me_type_malloced:1;    /* Nonzero if me_type was malloced. */
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
static GSList *mc_mount_list = NULL;
#endif /* HAVE_INFOMOUNT_LIST */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef STAT_STATVFS
/* Return true if statvfs works.  This is false for statvfs on systems
   with GNU libc on Linux kernels before 2.6.36, which stats all
   preceding entries in /proc/mounts; that makes df hang if even one
   of the corresponding file systems is hard-mounted but not available.  */
static int
statvfs_works (void)
{
#if ! (defined __linux__ && (defined __GLIBC__ || defined __UCLIBC__))
    return 1;
#else
    static int statvfs_works_cache = -1;
    struct utsname name;

    if (statvfs_works_cache < 0)
        statvfs_works_cache = (uname (&name) == 0 && 0 <= str_verscmp (name.release, "2.6.36"));
    return statvfs_works_cache;
#endif
}
#endif

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_INFOMOUNT_LIST
static void
free_mount_entry (struct mount_entry *me)
{
    if (me == NULL)
        return;
    g_free (me->me_devname);
    g_free (me->me_mountdir);
    g_free (me->me_mntroot);
    if (me->me_type_malloced)
        g_free (me->me_type);
    g_free (me);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef MOUNTED_GETMNTINFO       /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD */

#ifndef HAVE_STRUCT_STATFS_F_FSTYPENAME
static char *
fstype_to_string (short int t)
{
    switch (t)
    {
#ifdef MOUNT_PC
        /* cppcheck-suppress syntaxError */
    case MOUNT_PC:
        return "pc";
#endif
#ifdef MOUNT_MFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_MFS:
        return "mfs";
#endif
#ifdef MOUNT_LO
        /* cppcheck-suppress syntaxError */
    case MOUNT_LO:
        return "lo";
#endif
#ifdef MOUNT_TFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_TFS:
        return "tfs";
#endif
#ifdef MOUNT_TMP
        /* cppcheck-suppress syntaxError */
    case MOUNT_TMP:
        return "tmp";
#endif
#ifdef MOUNT_UFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_UFS:
        return "ufs";
#endif
#ifdef MOUNT_NFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_NFS:
        return "nfs";
#endif
#ifdef MOUNT_MSDOS
        /* cppcheck-suppress syntaxError */
    case MOUNT_MSDOS:
        return "msdos";
#endif
#ifdef MOUNT_LFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_LFS:
        return "lfs";
#endif
#ifdef MOUNT_LOFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_LOFS:
        return "lofs";
#endif
#ifdef MOUNT_FDESC
        /* cppcheck-suppress syntaxError */
    case MOUNT_FDESC:
        return "fdesc";
#endif
#ifdef MOUNT_PORTAL
        /* cppcheck-suppress syntaxError */
    case MOUNT_PORTAL:
        return "portal";
#endif
#ifdef MOUNT_NULL
        /* cppcheck-suppress syntaxError */
    case MOUNT_NULL:
        return "null";
#endif
#ifdef MOUNT_UMAP
        /* cppcheck-suppress syntaxError */
    case MOUNT_UMAP:
        return "umap";
#endif
#ifdef MOUNT_KERNFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_KERNFS:
        return "kernfs";
#endif
#ifdef MOUNT_PROCFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_PROCFS:
        return "procfs";
#endif
#ifdef MOUNT_AFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_AFS:
        return "afs";
#endif
#ifdef MOUNT_CD9660
        /* cppcheck-suppress syntaxError */
    case MOUNT_CD9660:
        return "cd9660";
#endif
#ifdef MOUNT_UNION
        /* cppcheck-suppress syntaxError */
    case MOUNT_UNION:
        return "union";
#endif
#ifdef MOUNT_DEVFS
        /* cppcheck-suppress syntaxError */
    case MOUNT_DEVFS:
        return "devfs";
#endif
#ifdef MOUNT_EXT2FS
        /* cppcheck-suppress syntaxError */
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

#ifdef MOUNTED_VMOUNT           /* AIX */
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

#if defined MOUNTED_GETMNTENT1 && (defined __linux__ || defined __ANDROID__)    /* GNU/Linux, Android */

/* Unescape the paths in mount tables.
   STR is updated in place.  */
static void
unescape_tab (char *str)
{
    size_t i, j = 0;
    size_t len;

    len = strlen (str) + 1;

    for (i = 0; i < len; i++)
    {
        if (str[i] == '\\' && (i + 4 < len)
            && str[i + 1] >= '0' && str[i + 1] <= '3'
            && str[i + 2] >= '0' && str[i + 2] <= '7' && str[i + 3] >= '0' && str[i + 3] <= '7')
        {
            str[j++] = (str[i + 1] - '0') * 64 + (str[i + 2] - '0') * 8 + (str[i + 3] - '0');
            i += 3;
        }
        else
            str[j++] = str[i];
    }
}
#endif

/* --------------------------------------------------------------------------------------------- */

/* Return a list of the currently mounted file systems, or NULL on error.
   Add each entry to the tail of the list so that they stay in order. */

static GSList *
read_file_system_list (void)
{
    GSList *mount_list = NULL;
    struct mount_entry *me;

#ifdef MOUNTED_GETMNTENT1       /* glibc, HP-UX, IRIX, Cygwin, Android,
                                   also (obsolete) 4.3BSD, SunOS */
    {
        FILE *fp;

#if defined __linux__ || defined __ANDROID__
        /* Try parsing mountinfo first, as that make device IDs available.
           Note we could use libmount routines to simplify this parsing a little
           (and that code is in previous versions of this function), however
           libmount depends on libselinux which pulls in many dependencies.  */
        char const *mountinfo = "/proc/self/mountinfo";

        fp = fopen (mountinfo, "r");
        if (fp != NULL)
        {
            char *line = NULL;
            size_t buf_size = 0;

            while (getline (&line, &buf_size, fp) != -1)
            {
                unsigned int devmaj, devmin;
                int target_s, target_e, type_s, type_e;
                int source_s, source_e, mntroot_s, mntroot_e;
                char test;
                char *dash;
                int rc;

                rc = sscanf (line, "%*u "       /* id - discarded  */
                             "%*u "     /* parent - discarded */
                             "%u:%u "   /* dev major:minor  */
                             "%n%*s%n " /* mountroot */
                             "%n%*s%n"  /* target, start and end  */
                             "%c",      /* more data...  */
                             &devmaj, &devmin, &mntroot_s, &mntroot_e, &target_s, &target_e, &test);

                if (rc != 3 && rc != 7) /* 7 if %n included in count.  */
                    continue;

                /* skip optional fields, terminated by " - "  */
                dash = strstr (line + target_e, " - ");
                if (dash == NULL)
                    continue;

                rc = sscanf (dash, " - "        /* */
                             "%n%*s%n " /* FS type, start and end  */
                             "%n%*s%n " /* source, start and end  */
                             "%c",      /* more data...  */
                             &type_s, &type_e, &source_s, &source_e, &test);
                if (rc != 1 && rc != 5) /* 5 if %n included in count.  */
                    continue;

                /* manipulate the sub-strings in place.  */
                line[mntroot_e] = '\0';
                line[target_e] = '\0';
                dash[type_e] = '\0';
                dash[source_e] = '\0';
                unescape_tab (dash + source_s);
                unescape_tab (line + target_s);
                unescape_tab (line + mntroot_s);

                me = g_malloc (sizeof *me);

                me->me_devname = g_strdup (dash + source_s);
                me->me_mountdir = g_strdup (line + target_s);
                me->me_mntroot = g_strdup (line + mntroot_s);
                me->me_type = g_strdup (dash + type_s);
                me->me_type_malloced = 1;
                me->me_dev = makedev (devmaj, devmin);
                /* we pass "false" for the "Bind" option as that's only
                   significant when the Fs_type is "none" which will not be
                   the case when parsing "/proc/self/mountinfo", and only
                   applies for static /etc/mtab files.  */
                me->me_dummy = ME_DUMMY (me->me_devname, me->me_type, FALSE);
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);

                mount_list = g_slist_prepend (mount_list, me);
            }

            free (line);

            if (ferror (fp) != 0)
            {
                int saved_errno = errno;

                fclose (fp);
                errno = saved_errno;
                goto free_then_fail;
            }

            if (fclose (fp) == EOF)
                goto free_then_fail;
        }
        else                    /* fallback to /proc/self/mounts (/etc/mtab).  */
#endif /* __linux __ || __ANDROID__ */
        {
            struct mntent *mnt;
            const char *table = MOUNTED;

            fp = setmntent (table, "r");
            if (fp == NULL)
                return NULL;

            while ((mnt = getmntent (fp)) != NULL)
            {
                gboolean bind;

                bind = hasmntopt (mnt, "bind") != NULL;

                me = g_malloc (sizeof (*me));
                me->me_devname = g_strdup (mnt->mnt_fsname);
                me->me_mountdir = g_strdup (mnt->mnt_dir);
                me->me_mntroot = NULL;
                me->me_type = g_strdup (mnt->mnt_type);
                me->me_type_malloced = 1;
                me->me_dummy = ME_DUMMY (me->me_devname, me->me_type, bind);
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = dev_from_mount_options (mnt->mnt_opts);

                mount_list = g_slist_prepend (mount_list, me);
            }

            if (endmntent (fp) == 0)
                goto free_then_fail;
        }
    }
#endif /* MOUNTED_GETMNTENT1. */

#ifdef MOUNTED_GETMNTINFO       /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD */
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
            me->me_mntroot = NULL;
            me->me_type = fs_type;
            me->me_type_malloced = 0;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            mount_list = g_slist_prepend (mount_list, me);
        }
    }
#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD, Minix */
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
            me->me_mntroot = NULL;
            me->me_type = g_strdup (fsp->f_fstypename);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            mount_list = g_slist_prepend (mount_list, me);
        }
    }
#endif /* MOUNTED_GETMNTINFO2 */

#if defined MOUNTED_FS_STAT_DEV /* Haiku, also (obsolete) BeOS */
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
        dirp = opendir (PATH_SEP_STR);
        if (dirp)
        {
            struct dirent *d;

            while ((d = readdir (dirp)) != NULL)
            {
                char *name;
                struct stat statbuf;

                if (DIR_IS_DOT (d->d_name))
                    continue;

                if (DIR_IS_DOTDOT (d->d_name))
                    name = g_strdup (PATH_SEP_STR);
                else
                    name = g_strconcat (PATH_SEP_STR, d->d_name, (char *) NULL);

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
                me->me_mntroot = NULL;
                me->me_type = g_strdup (fi.fsh_name);
                me->me_type_malloced = 1;
                me->me_dev = fi.dev;
                me->me_dummy = 0;
                me->me_remote = (fi.flags & B_FS_IS_SHARED) != 0;

                mount_list = g_slist_prepend (mount_list, me);
            }

        while (rootdir_list != NULL)
        {
            struct rootdir_entry *re = rootdir_list;

            rootdir_list = re->next;
            g_free (re->name);
            g_free (re);
        }
    }
#endif /* MOUNTED_FS_STAT_DEV */

#ifdef MOUNTED_GETFSSTAT        /*  OSF/1, also (obsolete) Apple Darwin 1.3 */
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
            me->me_mntroot = NULL;
            me->me_type = g_strdup (FS_TYPE (stats[counter]));
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */

            mount_list = g_slist_prepend (mount_list, me);
        }

        g_free (stats);
    }
#endif /* MOUNTED_GETFSSTAT */

#if defined MOUNTED_FREAD_FSTYP /* (obsolete) SVR3 */
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
            me->me_devname = g_strdup (mnt.mt_dev);
            me->me_mountdir = g_strdup (mnt.mt_filsys);
            me->me_mntroot = NULL;
            me->me_dev = (dev_t) (-1);  /* Magic; means not known yet. */
            me->me_type = "";
            me->me_type_malloced = 0;
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
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);

            mount_list = g_slist_prepend (mount_list, me);
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
#endif /* MOUNTED_FREAD_FSTYP */

#ifdef MOUNTED_GETEXTMNTENT     /* Solaris >= 8 */
    {
        struct extmnttab mnt;
        const char *table = MNTTAB;
        FILE *fp;
        int ret;

        /* No locking is needed, because the contents of /etc/mnttab is generated by the kernel. */

        errno = 0;
        fp = fopen (table, "r");
        if (fp == NULL)
            ret = errno;
        else
        {
            while ((ret = getextmntent (fp, &mnt, 1)) == 0)
            {
                me = g_malloc (sizeof *me);
                me->me_devname = g_strdup (mnt.mnt_special);
                me->me_mountdir = g_strdup (mnt.mnt_mountp);
                me->me_mntroot = NULL;
                me->me_type = g_strdup (mnt.mnt_fstype);
                me->me_type_malloced = 1;
                me->me_dummy = MNT_IGNORE (&mnt) != 0;
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = makedev (mnt.mnt_major, mnt.mnt_minor);

                mount_list = g_slist_prepend (mount_list, me);
            }

            ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
            /* Here ret = -1 means success, ret >= 0 means failure. */
        }

        if (ret >= 0)
        {
            errno = ret;
            goto free_then_fail;
        }
    }
#endif /* MOUNTED_GETEXTMNTENT */

#ifdef MOUNTED_GETMNTENT2       /* Solaris < 8, also (obsolete) SVR4 */
    {
        struct mnttab mnt;
        const char *table = MNTTAB;
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
        if (lockfd >= 0)
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
                me->me_mntroot = NULL;
                me->me_type = g_strdup (mnt.mnt_fstype);
                me->me_type_malloced = 1;
                me->me_dummy = MNT_IGNORE (&mnt) != 0;
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = dev_from_mount_options (mnt.mnt_mntopts);

                mount_list = g_slist_prepend (mount_list, me);
            }

            ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
            /* Here ret = -1 means success, ret >= 0 means failure. */
        }

        if (lockfd >= 0 && close (lockfd) != 0)
            ret = errno;

        if (ret >= 0)
        {
            errno = ret;
            goto free_then_fail;
        }
    }
#endif /* MOUNTED_GETMNTENT2.  */

#ifdef MOUNTED_VMOUNT           /* AIX */
    {
        int bufsize;
        void *entries;
        char *thisent;
        struct vmount *vmp;
        int n_entries;
        int i;

        /* Ask how many bytes to allocate for the mounted file system info.  */
        entries = &bufsize;
        if (mntctl (MCTL_QUERY, sizeof (bufsize), entries) != 0)
            return NULL;
        entries = g_malloc (bufsize);

        /* Get the list of mounted file systems.  */
        n_entries = mntctl (MCTL_QUERY, bufsize, entries);
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
            me->me_mntroot = NULL;
            me->me_type = g_strdup (fstype_to_string (vmp->vmt_gfstype));
            me->me_type_malloced = 1;
            options = thisent + vmp->vmt_data[VMT_ARGS].vmt_off;
            ignore = strstr (options, "ignore");
            me->me_dummy = (ignore
                            && (ignore == options || ignore[-1] == ',')
                            && (ignore[sizeof ("ignore") - 1] == ','
                                || ignore[sizeof ("ignore") - 1] == '\0'));
            me->me_dev = (dev_t) (-1);  /* vmt_fsid might be the info we want.  */

            mount_list = g_slist_prepend (mount_list, me);
        }
        g_free (entries);
    }
#endif /* MOUNTED_VMOUNT. */

#ifdef MOUNTED_INTERIX_STATVFS  /* Interix */
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
                me->me_mntroot = NULL;
                me->me_type = g_strdup (dev.f_fstypename);
                me->me_type_malloced = 1;
                me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
                me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
                me->me_dev = (dev_t) (-1);      /* Magic; means not known yet. */

                mount_list = g_slist_prepend (mount_list, me);
            }
        }
        closedir (dirp);
    }
#endif /* MOUNTED_INTERIX_STATVFS */

    return g_slist_reverse (mount_list);

  free_then_fail:
    {
        int saved_errno = errno;

        g_slist_free_full (mount_list, (GDestroyNotify) free_mount_entry);

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
 ** structure. See my_statfs() below for the "other side" of this hack.
 */

static GSList *
read_file_system_list (void)
{
    struct _disk_entry de;
    struct statfs fs;
    int i, fd;
    char *tp, dev[_POSIX_NAME_MAX], dir[_POSIX_PATH_MAX];
    struct mount_entry *me = NULL;
    static GSList *list = NULL;

    if (list != NULL)
    {
        me = (struct mount_entry *) list->data;

        g_free (me->me_devname);
        g_free (me->me_mountdir);
        g_free (me->me_mntroot);
        g_free (me->me_type);
    }
    else
    {
        me = (struct mount_entry *) g_malloc (sizeof (struct mount_entry));
        list = g_slist_prepend (list, me);
    }

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
    me->me_mntroot = NULL;
    me->me_type = g_strdup (tp);
    me->me_dev = de.disk_type;

#ifdef DEBUG
    fprintf (stderr,
             "disk_get_entry():\n\tdisk_type=%d (%s)\n\tdriver_name='%-*.*s'\n\tdisk_drv=%d\n",
             de.disk_type, tp, _DRIVER_NAME_LEN, _DRIVER_NAME_LEN, de.driver_name, de.disk_drv);
    fprintf (stderr, "fsys_get_mount_dev():\n\tdevice='%s'\n", dev);
    fprintf (stderr, "fsys_get_mount_pt():\n\tmount point='%s'\n", dir);
#endif /* DEBUG */

    return (list);
}
#endif /* HAVE_INFOMOUNT_QNX */

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

#elif defined STAT_STATFS4      /* SVR3, old Irix */

        struct statfs fsd;

        if (statfs (file, &fsd, sizeof (fsd), 0) < 0)
            return -1;

        /* Empirically, the block counts on most SVR3 and SVR3-derived
           systems seem to always be in terms of 512-byte blocks,
           no matter what value f_bsize has.  */
        fsp->fsu_blocksize = 512;
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
    g_clear_slist (&mc_mount_list, (GDestroyNotify) free_mount_entry);
#endif /* HAVE_INFOMOUNT_LIST */
}

/* --------------------------------------------------------------------------------------------- */

void
init_my_statfs (void)
{
#ifdef HAVE_INFOMOUNT_LIST
    free_my_statfs ();
    mc_mount_list = read_file_system_list ();
#endif /* HAVE_INFOMOUNT_LIST */
}

/* --------------------------------------------------------------------------------------------- */

void
my_statfs (struct my_statfs *myfs_stats, const char *path)
{
#ifdef HAVE_INFOMOUNT_LIST
    size_t len = 0;
    struct mount_entry *entry = NULL;
    GSList *temp;
    struct fs_usage fs_use;

    for (temp = mc_mount_list; temp != NULL; temp = g_slist_next (temp))
    {
        struct mount_entry *me;
        size_t i;

        me = (struct mount_entry *) temp->data;
        i = strlen (me->me_mountdir);
        if (i > len && (strncmp (path, me->me_mountdir, i) == 0) &&
            (entry == NULL || IS_PATH_SEP (path[i]) || path[i] == '\0'))
        {
            len = i;
            entry = me;
        }
    }

    if (entry != NULL)
    {
        memset (&fs_use, 0, sizeof (fs_use));
        get_fs_usage (entry->me_mountdir, NULL, &fs_use);

        myfs_stats->type = entry->me_dev;
        myfs_stats->typename = entry->me_type;
        myfs_stats->mpoint = entry->me_mountdir;
        myfs_stats->mroot = entry->me_mntroot;
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
         ** This is the "other side" of the hack to read_file_system_list() above.
         ** It's not the most efficient approach, but consumes less memory. It
         ** also accommodates QNX's ability to mount filesystems on the fly.
         */
        struct mount_entry *entry;
    struct fs_usage fs_use;

    entry = read_file_system_list ();
    if (entry != NULL)
    {
        get_fs_usage (entry->me_mountdir, NULL, &fs_use);

        myfs_stats->type = entry->me_dev;
        myfs_stats->typename = entry->me_type;
        myfs_stats->mpoint = entry->me_mountdir;
        myfs_stats->mroot = entry->me_mntroot;
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
