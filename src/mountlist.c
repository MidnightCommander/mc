/* mountlist.c -- return a list of mounted filesystems
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>

/* This header needs to be included before sys/mount.h on *BSD */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined (MOUNTED_GETFSSTAT)	/* __alpha running OSF_1 */
#include <sys/mount.h>
#include <sys/fs_types.h>
#endif				/* MOUNTED_GETFSSTAT */

#ifdef MOUNTED_GETMNTENT1	/* 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
#include <mntent.h>
#if !defined(MOUNTED)
#if defined(MNT_MNTTAB)		/* HP-UX.  */
#define MOUNTED MNT_MNTTAB
#endif
#if defined(MNTTABNAME)		/* Dynix.  */
#define MOUNTED MNTTABNAME
#endif
#endif
#endif

#ifdef MOUNTED_GETMNTINFO	/* 4.4BSD.  */
#include <sys/mount.h>
#endif

#ifdef MOUNTED_GETMNT		/* Ultrix.  */
#include <sys/mount.h>
#include <sys/fs_types.h>
#endif

#ifdef MOUNTED_FREAD		/* SVR2.  */
#include <mnttab.h>
#endif

#ifdef MOUNTED_FREAD_FSTYP	/* SVR3.  */
#include <mnttab.h>
#include <sys/fstyp.h>
#include <sys/statfs.h>
#endif

#ifdef MOUNTED_GETMNTENT2	/* SVR4.  */
#include <sys/mnttab.h>
#endif

#ifdef MOUNTED_VMOUNT		/* AIX.  */
#include <fshelp.h>
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_INFOMOUNT_QNX
#include <sys/disk.h>
#include <sys/fsys.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_FILSYS_H
#include <sys/filsys.h>		/* SVR2.  */
#endif

#ifdef HAVE_DUSTAT_H		/* AIX PS/2.  */
#include <sys/dustat.h>
#endif

#ifdef HAVE_SYS_STATVFS_H	/* SVR4.  */
#include <sys/statvfs.h>
#endif

#include "global.h"
#include "mountlist.h"
#include "util.h"

#ifdef DOLPHIN
/* So special that it's not worth putting this in autoconf.  */
#undef MOUNTED_FREAD_FSTYP
#define MOUNTED_GETMNTTBL
#endif

#if defined (__QNX__) && !defined(__QNXNTO__) && !defined (HAVE_INFOMOUNT_LIST)
#    define HAVE_INFOMOUNT_QNX
#endif

#if defined(HAVE_INFOMOUNT_LIST) || defined(HAVE_INFOMOUNT_QNX)
#    define HAVE_INFOMOUNT
#endif

/* A mount table entry. */
struct mount_entry
{
  char *me_devname;		/* Device node pathname, including "/dev/". */
  char *me_mountdir;		/* Mount point directory pathname. */
  char *me_type;		/* "nfs", "4.2", etc. */
  dev_t me_dev;			/* Device number of me_mountdir. */
  struct mount_entry *me_next;
};

struct fs_usage
{
  long fsu_blocks;		/* Total blocks. */
  long fsu_bfree;		/* Free blocks available to superuser. */
  long fsu_bavail;		/* Free blocks available to non-superuser. */
  long fsu_files;		/* Total file nodes. */
  long fsu_ffree;		/* Free file nodes. */
};

static int get_fs_usage (char *path, struct fs_usage *fsp);

#ifdef HAVE_INFOMOUNT_LIST

static struct mount_entry *mount_list = NULL;

#ifdef MOUNTED_GETMNTENT1	/* 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
/* Return the value of the hexadecimal number represented by CP.
   No prefix (like '0x') or suffix (like 'h') is expected to be
   part of CP. */

static int xatoi (const char *cp)
{
    int val;

    val = 0;
    while (*cp) {
	if (*cp >= 'a' && *cp <= 'f')
	    val = val * 16 + *cp - 'a' + 10;
	else if (*cp >= 'A' && *cp <= 'F')
	    val = val * 16 + *cp - 'A' + 10;
	else if (*cp >= '0' && *cp <= '9')
	    val = val * 16 + *cp - '0';
	else
	    break;
	cp++;
    }
    return val;
}
#endif /* MOUNTED_GETMNTENT1 */

#if defined (MOUNTED_GETMNTINFO) && !defined (HAVE_F_FSTYPENAME)
static char *fstype_to_string (short t)
{
    switch (t) {
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
     return "ufs" ;
#endif
#ifdef MOUNT_NFS
   case MOUNT_NFS:
     return "nfs" ;
#endif
#ifdef MOUNT_MSDOS
   case MOUNT_MSDOS:
     return "msdos" ;
#endif
#ifdef MOUNT_LFS
   case MOUNT_LFS:
     return "lfs" ;
#endif
#ifdef MOUNT_LOFS
   case MOUNT_LOFS:
     return "lofs" ;
#endif
#ifdef MOUNT_FDESC
   case MOUNT_FDESC:
     return "fdesc" ;
#endif
#ifdef MOUNT_PORTAL
   case MOUNT_PORTAL:
     return "portal" ;
#endif
#ifdef MOUNT_NULL
   case MOUNT_NULL:
     return "null" ;
#endif
#ifdef MOUNT_UMAP
   case MOUNT_UMAP:
     return "umap" ;
#endif
#ifdef MOUNT_KERNFS
   case MOUNT_KERNFS:
     return "kernfs" ;
#endif
#ifdef MOUNT_PROCFS
   case MOUNT_PROCFS:
     return "procfs" ;
#endif
#ifdef MOUNT_AFS
   case MOUNT_AFS:
     return "afs" ;
#endif
#ifdef MOUNT_CD9660
   case MOUNT_CD9660:
     return "cd9660" ;
#endif
#ifdef MOUNT_UNION
   case MOUNT_UNION:
     return "union" ;
#endif
#ifdef MOUNT_DEVFS
   case MOUNT_DEVFS:
     return "devfs" ;
#endif
#ifdef MOUNT_EXT2FS
   case MOUNT_EXT2FS:
     return "ext2fs" ;
#endif
    default:
	return "?";
    }
}
#endif /* MOUNTED_GETMNTINFO && !HAVE_F_FSTYPENAME */

#ifdef MOUNTED_VMOUNT		/* AIX.  */
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

/* Return a list of the currently mounted filesystems, or NULL on error.
   Add each entry to the tail of the list so that they stay in order.
   If NEED_FS_TYPE is nonzero, ensure that the filesystem type fields in
   the returned list are valid.  Otherwise, they might not be.
   If ALL_FS is zero, do not return entries for filesystems that
   are automounter (dummy) entries.  */

static struct mount_entry *
read_filesystem_list (int need_fs_type, int all_fs)
{
    struct mount_entry *mlist;
    struct mount_entry *me;
    struct mount_entry *mtail;

    (void) need_fs_type;
    (void) all_fs;

    /* Start the list off with a dummy entry. */
    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
    me->me_next = NULL;
    mlist = mtail = me;

#ifdef MOUNTED_GETMNTENT1	/* 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
#ifdef MOUNTED
    {
	struct mntent *mnt;
	FILE *fp;
	const char *devopt;

	fp = setmntent (MOUNTED, "r");
	if (fp == NULL)
	    return NULL;

	while ((mnt = getmntent (fp))) {
	    if (!all_fs && (!strcmp (mnt->mnt_type, "ignore")
			    || !strcmp (mnt->mnt_type, "auto")))
		continue;

	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup (mnt->mnt_fsname);
	    me->me_mountdir = strdup (mnt->mnt_dir);
	    me->me_type = strdup (mnt->mnt_type);
	    devopt = strstr (mnt->mnt_opts, "dev=");
	    if (devopt) {
		if (devopt[4] == '0' && (devopt[5] == 'x' || devopt[5] == 'X'))
		    me->me_dev = xatoi (devopt + 6);
		else
		    me->me_dev = xatoi (devopt + 4);
	    } else
		me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}

	if (endmntent (fp) == 0)
	    return NULL;
    }
#endif /* MOUNTED */
#endif /* MOUNTED_GETMNTENT1 */

#ifdef MOUNTED_GETMNTINFO	/* 4.4BSD.  */
    {
	struct statfs *fsp;
	int entries;

	entries = getmntinfo (&fsp, MNT_NOWAIT);
	if (entries < 0)
	    return NULL;
	while (entries-- > 0) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup (fsp->f_mntfromname);
	    me->me_mountdir = strdup (fsp->f_mntonname);
#ifdef HAVE_F_FSTYPENAME
	    me->me_type = strdup (fsp->f_fstypename);
#else
	    me->me_type = fstype_to_string (fsp->f_type);
#endif
	    me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	    fsp++;
	}
    }
#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_GETMNT		/* Ultrix.  */
    {
	int offset = 0;
	int val;
	struct fs_data fsd;

	while ((val = getmnt (&offset, &fsd, sizeof (fsd), NOSTAT_MANY,
			      NULL)) > 0) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup (fsd.fd_req.devname);
	    me->me_mountdir = strdup (fsd.fd_req.path);
	    me->me_type = gt_names[fsd.fd_req.fstype];
	    me->me_dev = fsd.fd_req.dev;
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}
	if (val < 0)
	    return NULL;
    }
#endif /* MOUNTED_GETMNT */

#ifdef MOUNTED_GETFSSTAT	/* __alpha running OSF_1 */
    {
	int numsys, counter, bufsize;
	struct statfs *stats;

	numsys = getfsstat ((struct statfs *) 0, 0L, MNT_WAIT);
	if (numsys < 0)
	    return (NULL);

	bufsize = (1 + numsys) * sizeof (struct statfs);
	stats = (struct statfs *) malloc (bufsize);
	numsys = getfsstat (stats, bufsize, MNT_WAIT);

	if (numsys < 0) {
	    free (stats);
	    return (NULL);
	}
	for (counter = 0; counter < numsys; counter++) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup (stats[counter].f_mntfromname);
	    me->me_mountdir = strdup (stats[counter].f_mntonname);
	    me->me_type = mnt_names[stats[counter].f_type];
	    me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}

	free (stats);
    }
#endif /* MOUNTED_GETFSSTAT */

#if defined (MOUNTED_FREAD) || defined (MOUNTED_FREAD_FSTYP)	/* SVR[23].  */
    {
	struct mnttab mnt;
	char *table = "/etc/mnttab";
	FILE *fp;

	fp = fopen (table, "r");
	if (fp == NULL)
	    return NULL;

	while (fread (&mnt, sizeof mnt, 1, fp) > 0) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
#ifdef GETFSTYP			/* SVR3.  */
	    me->me_devname = strdup (mnt.mt_dev);
#else
	    me->me_devname = malloc (strlen (mnt.mt_dev) + 6);
	    strcpy (me->me_devname, "/dev/");
	    strcpy (me->me_devname + 5, mnt.mt_dev);
#endif
	    me->me_mountdir = strdup (mnt.mt_filsys);
	    me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_type = "";
#ifdef GETFSTYP			/* SVR3.  */
	    if (need_fs_type) {
		struct statfs fsd;
		char typebuf[FSTYPSZ];

		if (statfs (me->me_mountdir, &fsd, sizeof fsd, 0) != -1
		    && sysfs (GETFSTYP, fsd.f_fstyp, typebuf) != -1)
		    me->me_type = strdup (typebuf);
	    }
#endif
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}

	if (fclose (fp) == EOF)
	    return NULL;
    }
#endif /* MOUNTED_FREAD || MOUNTED_FREAD_FSTYP */

#ifdef MOUNTED_GETMNTTBL	/* DolphinOS goes it's own way */
    {
	struct mntent **mnttbl = getmnttbl (), **ent;
	for (ent = mnttbl; *ent; ent++) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup ((*ent)->mt_resource);
	    me->me_mountdir = strdup ((*ent)->mt_directory);
	    me->me_type = strdup ((*ent)->mt_fstype);
	    me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}
	endmnttbl ();
    }
#endif /* MOUNTED_GETMNTTBL */

#ifdef MOUNTED_GETMNTENT2	/* SVR4.  */
    {
	struct mnttab mnt;
	char *table = MNTTAB;
	FILE *fp;
	int ret;

	fp = fopen (table, "r");
	if (fp == NULL)
	    return NULL;

	while ((ret = getmntent (fp, &mnt)) == 0) {
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    me->me_devname = strdup (mnt.mnt_special);
	    me->me_mountdir = strdup (mnt.mnt_mountp);
	    me->me_type = strdup (mnt.mnt_fstype);
	    me->me_dev = -1;	/* Magic; means not known yet. */
	    me->me_next = NULL;
	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}

	if (ret > 0)
	    return NULL;
	if (fclose (fp) == EOF)
	    return NULL;
    }
#endif /* MOUNTED_GETMNTENT2 */

#ifdef MOUNTED_VMOUNT		/* AIX.  */
    {
	int bufsize;
	char *entries, *thisent;
	struct vmount *vmp;

	/* Ask how many bytes to allocate for the mounted filesystem info.  */
	mntctl (MCTL_QUERY, sizeof bufsize, (struct vmount *) &bufsize);
	entries = malloc (bufsize);

	/* Get the list of mounted filesystems.  */
	mntctl (MCTL_QUERY, bufsize, (struct vmount *) entries);

	for (thisent = entries; thisent < entries + bufsize;
	     thisent += vmp->vmt_length) {
	    vmp = (struct vmount *) thisent;
	    me = (struct mount_entry *) malloc (sizeof (struct mount_entry));
	    if (vmp->vmt_flags & MNT_REMOTE) {
		char *host, *path;

		/* Prepend the remote pathname.  */
		host = thisent + vmp->vmt_data[VMT_HOSTNAME].vmt_off;
		path = thisent + vmp->vmt_data[VMT_OBJECT].vmt_off;
		me->me_devname = malloc (strlen (host) + strlen (path) + 2);
		strcpy (me->me_devname, host);
		strcat (me->me_devname, ":");
		strcat (me->me_devname, path);
	    } else {
		me->me_devname = strdup (thisent +
				      vmp->vmt_data[VMT_OBJECT].vmt_off);
	    }
	    me->me_mountdir = strdup (thisent + vmp->vmt_data[VMT_STUB].vmt_off);
	    me->me_type = strdup (fstype_to_string (vmp->vmt_gfstype));
	    me->me_dev = -1;	/* vmt_fsid might be the info we want.  */
	    me->me_next = NULL;

	    /* Add to the linked list. */
	    mtail->me_next = me;
	    mtail = me;
	}
	free (entries);
    }
#endif /* MOUNTED_VMOUNT */

    /* Free the dummy head. */
    me = mlist;
    mlist = mlist->me_next;
    free (me);
    return mlist;
}
#endif /* HAVE_INFOMOUNT_LIST */

#ifdef HAVE_INFOMOUNT_QNX
/*
** QNX has no [gs]etmnt*(), [gs]etfs*(), or /etc/mnttab, but can do
** this via the following code.
** Note that, as this is based on CWD, it only fills one mount_entry
** structure. See my_statfs() in utilunix.c for the "other side" of
** this hack.
*/

static struct mount_entry *
read_filesystem_list(int need_fs_type, int all_fs)
{
	struct _disk_entry	de;
	struct statfs		fs;
	int					i, fd;
	char				*tp, dev[_POSIX_NAME_MAX], dir[_POSIX_PATH_MAX];

	static struct mount_entry	*me = NULL;

	if (me)
	{
		if (me->me_devname) free(me->me_devname);
		if (me->me_mountdir) free(me->me_mountdir);
		if (me->me_type) free(me->me_type);
	}
	else
		me = (struct mount_entry *)malloc(sizeof(struct mount_entry));

	if (!getcwd(dir, _POSIX_PATH_MAX)) return (NULL);

	if ((fd = open(dir, O_RDONLY)) == -1) return (NULL);

	i = disk_get_entry(fd, &de);

	close(fd);

	if (i == -1) return (NULL);

	switch (de.disk_type)
	{
		case _UNMOUNTED:	tp = "unmounted";	break;
		case _FLOPPY:		tp = "Floppy";		break;
		case _HARD:			tp = "Hard";		break;
		case _RAMDISK:		tp = "Ram";			break;
		case _REMOVABLE:	tp = "Removable";	break;
		case _TAPE:			tp = "Tape";		break;
		case _CDROM:		tp = "CDROM";		break;
		default:			tp = "unknown";
	}

	if (fsys_get_mount_dev(dir, &dev) == -1) return (NULL);

	if (fsys_get_mount_pt(dev, &dir) == -1) return (NULL);

	me->me_devname = strdup(dev);
	me->me_mountdir = strdup(dir);
	me->me_type = strdup(tp);
	me->me_dev = de.disk_type;

#ifdef DEBUG
	fprintf(stderr, "disk_get_entry():\n\tdisk_type=%d (%s)\n\tdriver_name='%-*.*s'\n\tdisk_drv=%d\n",
		de.disk_type, tp, _DRIVER_NAME_LEN, _DRIVER_NAME_LEN, de.driver_name, de.disk_drv);
	fprintf(stderr, "fsys_get_mount_dev():\n\tdevice='%s'\n", dev);
	fprintf(stderr, "fsys_get_mount_pt():\n\tmount point='%s'\n", dir);
#endif /* DEBUG */

	return (me);
}
#endif /* HAVE_INFOMOUNT_QNX */

void
init_my_statfs (void)
{
#ifdef HAVE_INFOMOUNT_LIST
    mount_list = read_filesystem_list (1, 1);
#endif /* HAVE_INFOMOUNT_LIST */
}

void
my_statfs (struct my_statfs *myfs_stats, const char *path)
{
#ifdef HAVE_INFOMOUNT_LIST
    int i, len = 0;
    struct mount_entry *entry = NULL;
    struct mount_entry *temp = mount_list;
    struct fs_usage fs_use;

    while (temp){
	i = strlen (temp->me_mountdir);
	if (i > len && (strncmp (path, temp->me_mountdir, i) == 0))
	    if (!entry || (path [i] == PATH_SEP || path [i] == 0)){
		len = i;
		entry = temp;
	    }
	temp = temp->me_next;
    }

    if (entry){
	memset (&fs_use, 0, sizeof (struct fs_usage));
	get_fs_usage (entry->me_mountdir, &fs_use);

	myfs_stats->type = entry->me_dev;
	myfs_stats->typename = entry->me_type;
	myfs_stats->mpoint = entry->me_mountdir;
	myfs_stats->device = entry->me_devname;
	myfs_stats->avail = getuid () ? fs_use.fsu_bavail/2 : fs_use.fsu_bfree/2;
	myfs_stats->total = fs_use.fsu_blocks/2;
	myfs_stats->nfree = fs_use.fsu_ffree;
	myfs_stats->nodes = fs_use.fsu_files;
    } else
#endif /* HAVE_INFOMOUNT_LIST */

#ifdef HAVE_INFOMOUNT_QNX
/*
** This is the "other side" of the hack to read_filesystem_list() in
** mountlist.c.
** It's not the most efficient approach, but consumes less memory. It
** also accomodates QNX's ability to mount filesystems on the fly.
*/
	struct mount_entry	*entry;
    struct fs_usage		fs_use;

	if ((entry = read_filesystem_list(0, 0)) != NULL)
	{
		get_fs_usage(entry->me_mountdir, &fs_use);

		myfs_stats->type = entry->me_dev;
		myfs_stats->typename = entry->me_type;
		myfs_stats->mpoint = entry->me_mountdir;
		myfs_stats->device = entry->me_devname;

		myfs_stats->avail = fs_use.fsu_bfree / 2;
		myfs_stats->total = fs_use.fsu_blocks / 2;
		myfs_stats->nfree = fs_use.fsu_ffree;
		myfs_stats->nodes = fs_use.fsu_files;
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

#ifdef HAVE_INFOMOUNT

/* Return the number of TOSIZE-byte blocks used by
   BLOCKS FROMSIZE-byte blocks, rounding away from zero.
   TOSIZE must be positive.  Return -1 if FROMSIZE is not positive.  */

static long
fs_adjust_blocks (long blocks, int fromsize, int tosize)
{
    if (tosize <= 0)
	abort ();
    if (fromsize <= 0)
	return -1;

    if (fromsize == tosize)	/* E.g., from 512 to 512.  */
	return blocks;
    else if (fromsize > tosize)	/* E.g., from 2048 to 512.  */
	return blocks * (fromsize / tosize);
    else			/* E.g., from 256 to 512.  */
	return (blocks + (blocks < 0 ? -1 : 1)) / (tosize / fromsize);
}

#if defined(_AIX) && defined(_I386)
/* AIX PS/2 does not supply statfs.  */
static int aix_statfs (char *path, struct statfs *fsb)
{
    struct stat stats;
    struct dustat fsd;

    if (stat (path, &stats))
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
#define statfs(path,fsb) aix_statfs(path,fsb)
#endif				/* _AIX && _I386 */

/* Fill in the fields of FSP with information about space usage for
   the filesystem on which PATH resides.
   Return 0 if successful, -1 if not. */

static int
get_fs_usage (char *path, struct fs_usage *fsp)
{
#ifdef STAT_STATFS3_OSF1
    struct statfs fsd;

    if (statfs (path, &fsd, sizeof (struct statfs)) != 0)
	 return -1;
#define CONVERT_BLOCKS(b) fs_adjust_blocks ((b), fsd.f_fsize, 512)
#endif				/* STAT_STATFS3_OSF1 */

#ifdef STAT_STATFS2_FS_DATA	/* Ultrix.  */
    struct fs_data fsd;

    if (statfs (path, &fsd) != 1)
	return -1;
#define CONVERT_BLOCKS(b) fs_adjust_blocks ((b), 1024, 512)
    fsp->fsu_blocks = CONVERT_BLOCKS (fsd.fd_req.btot);
    fsp->fsu_bfree = CONVERT_BLOCKS (fsd.fd_req.bfree);
    fsp->fsu_bavail = CONVERT_BLOCKS (fsd.fd_req.bfreen);
    fsp->fsu_files = fsd.fd_req.gtot;
    fsp->fsu_ffree = fsd.fd_req.gfree;
#endif

#ifdef STAT_STATFS2_BSIZE	/* 4.3BSD, SunOS 4, HP-UX, AIX.  */
    struct statfs fsd;

    if (statfs (path, &fsd) < 0)
	return -1;
#define CONVERT_BLOCKS(b) fs_adjust_blocks ((b), fsd.f_bsize, 512)
#endif

#ifdef STAT_STATFS2_FSIZE	/* 4.4BSD.  */
    struct statfs fsd;

    if (statfs (path, &fsd) < 0)
	return -1;
#define CONVERT_BLOCKS(b) fs_adjust_blocks ((b), fsd.f_fsize, 512)
#endif

#ifdef STAT_STATFS4		/* SVR3, Dynix, Irix, AIX.  */
    struct statfs fsd;

    if (statfs (path, &fsd, sizeof fsd, 0) < 0)
	return -1;
    /* Empirically, the block counts on most SVR3 and SVR3-derived
       systems seem to always be in terms of 512-byte blocks,
       no matter what value f_bsize has.  */
#if _AIX
#define CONVERT_BLOCKS(b) fs_adjust_blocks ((b), fsd.f_bsize, 512)
#else
#define CONVERT_BLOCKS(b) (b)
#ifndef _SEQUENT_		/* _SEQUENT_ is DYNIX/ptx.  */
#ifndef DOLPHIN			/* DOLPHIN 3.8.alfa/7.18 has f_bavail */
#define f_bavail f_bfree
#endif
#endif
#endif
#endif

#ifdef STAT_STATVFS		/* SVR4.  */
    struct statvfs fsd;

    if (statvfs (path, &fsd) < 0)
	return -1;
    /* f_frsize isn't guaranteed to be supported.  */
#define CONVERT_BLOCKS(b) \
  fs_adjust_blocks ((b), fsd.f_frsize ? fsd.f_frsize : fsd.f_bsize, 512)
#endif

#if defined(CONVERT_BLOCKS) && !defined(STAT_STATFS2_FS_DATA) && !defined(STAT_READ_FILSYS)	/* !Ultrix && !SVR2.  */
    fsp->fsu_blocks = CONVERT_BLOCKS (fsd.f_blocks);
    fsp->fsu_bfree = CONVERT_BLOCKS (fsd.f_bfree);
    fsp->fsu_bavail = CONVERT_BLOCKS (fsd.f_bavail);
    fsp->fsu_files = fsd.f_files;
    fsp->fsu_ffree = fsd.f_ffree;
#endif

    return 0;
}

#endif /* HAVE_INFOMOUNT */
