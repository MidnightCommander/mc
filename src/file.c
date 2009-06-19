/* File management.
   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   
   Written by: 1994, 1995       Janne Kukonlehto
               1994, 1995       Fred Leeflang
               1994, 1995, 1996 Miguel de Icaza
               1995, 1996       Jakub Jelinek
	       1997             Norbert Warmuth
	       1998		Pavel Machek

   The copy code was based in GNU's cp, and was written by:
   Torbjorn Granlund, David MacKenzie, and Jim Meyering.

   The move code was based in GNU's mv, and was written by:
   Mike Parker and David MacKenzie.

   Janne Kukonlehto added much error recovery to them for being used
   in an interactive program.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/*
 * Please note that all dialogs used here must be safe for background
 * operations.
 */

/** \file  file.c
 *  \brief Source: file management
 */

/* {{{ Include files */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include "global.h"
#include "tty.h"
#include "setup.h"
#include "color.h"
#include "win.h"
#include "dialog.h"
#include "widget.h"
#include "main.h"		/* cmd_buf */
#include "layout.h"
#include "widget.h"
#include "wtools.h"
#include "background.h"		/* we_are_background */
#include "../src/strescape.h"
#include "strutil.h"
#include "../src/search/search.h"

/* Needed for current_panel, other_panel and WTree */
#include "dir.h"
#include "panel.h"
#include "file.h"
#include "filegui.h"
#include "tree.h"
#include "key.h"
#include "../vfs/vfs-impl.h"

/* }}} */

/* Hack: the vfs code should not rely on this */
#define WITH_FULL_PATHS 1

int verbose = 1;

/*
 * Whether the Midnight Commander tries to provide more
 * information about copy/move sizes and bytes transfered
 * at the expense of some speed
 */
int file_op_compute_totals = 1;

/* This is a hard link cache */
struct link {
    struct link *next;
    struct vfs_class *vfs;
    dev_t dev;
    ino_t ino;
    short linkcount;
    mode_t st_mode;
    char name[1];
};

/* the hard link cache */
static struct link *linklist = NULL;

/* the files-to-be-erased list */
static struct link *erase_list;

/*
 * In copy_dir_dir we use two additional single linked lists: The first - 
 * variable name `parent_dirs' - holds information about already copied 
 * directories and is used to detect cyclic symbolic links. 
 * The second (`dest_dirs' below) holds information about just created 
 * target directories and is used to detect when an directory is copied 
 * into itself (we don't want to copy infinitly). 
 * Both lists don't use the linkcount and name structure members of struct
 * link.
 */
static struct link *dest_dirs = 0;

const char *op_names[3] = {
    N_(" Copy "),
    N_(" Move "),
    N_(" Delete ")
};

/* }}} */

static FileProgressStatus query_replace (FileOpContext * ctx, const char *destname,
					    struct stat *_s_stat, struct stat *_d_stat);
static FileProgressStatus query_recursive (FileOpContext * ctx, const char *s);
static FileProgressStatus do_file_error (const char *str);
static FileProgressStatus erase_dir_iff_empty (FileOpContext *ctx, const char *s);
static FileProgressStatus erase_file (FileOpContext *ctx, const char *s,
					off_t *progress_count, double *progress_bytes,
					int is_toplevel_file);
static FileProgressStatus files_error (const char *format, const char *file1,
					const char *file2);


enum CaseConvs { NO_CONV = 0, UP_CHAR = 1, LOW_CHAR = 2, UP_SECT =
	4, LOW_SECT = 8 };

static FileProgressStatus transform_error = FILE_CONT;

static char *
transform_source (FileOpContext *ctx, const char *source)
{
    char *s = g_strdup (source);
    char *q;
    const char *fnsource = x_basename (s);

    /* We remove \n from the filename since regex routines would use \n as an anchor */
    /* this is just to be allowed to maniupulate file names with \n on it */
    for (q = s; *q; q++) {
	if (*q == '\n')
	    *q = ' ';
    }

    str_fix_string (fnsource);

    if ( ! mc_search_run(ctx->search_handle, fnsource, 0, strlen (fnsource), NULL) ){
	transform_error = FILE_SKIP;
	g_free (s);
	return NULL;
    }
    g_free (s);
    return mc_search_prepare_replace_str2 (ctx->search_handle, ctx->dest_mask);
}

static void
free_linklist (struct link **linklist)
{
    struct link *lp, *lp2;

    for (lp = *linklist; lp != NULL; lp = lp2) {
	lp2 = lp->next;
	g_free (lp);
    }
    *linklist = NULL;
}

static int
is_in_linklist (struct link *lp, const char *path, struct stat *sb)
{
    ino_t ino = sb->st_ino;
    dev_t dev = sb->st_dev;
#ifdef USE_VFS
    struct vfs_class *vfs = vfs_get_class (path);
#endif				/* USE_VFS */

    while (lp) {
#ifdef USE_VFS
	if (lp->vfs == vfs)
#endif				/* USE_VFS */
	    if (lp->ino == ino && lp->dev == dev)
		return 1;
	lp = lp->next;
    }
    return 0;
}

/*
 * Returns 0 if the inode wasn't found in the cache and 1 if it was found
 * and a hardlink was succesfully made
 */
static int
check_hardlinks (const char *src_name, const char *dst_name, struct stat *pstat)
{
    struct link *lp;
    struct vfs_class *my_vfs = vfs_get_class (src_name);
    ino_t ino = pstat->st_ino;
    dev_t dev = pstat->st_dev;
    struct stat link_stat;
    const char *p;

    if (vfs_file_class_flags (src_name) & VFSF_NOLINKS)
	return 0;

    for (lp = linklist; lp != NULL; lp = lp->next)
	if (lp->vfs == my_vfs && lp->ino == ino && lp->dev == dev) {
	    if (!mc_stat (lp->name, &link_stat) && link_stat.st_ino == ino
		&& link_stat.st_dev == dev
		&& vfs_get_class (lp->name) == my_vfs) {
		p = strchr (lp->name, 0) + 1;	/* i.e. where the `name' file
						   was copied to */
		if (vfs_get_class (dst_name) == vfs_get_class (p)) {
		    if (!mc_stat (p, &link_stat)) {
			if (!mc_link (p, dst_name))
			    return 1;
		    }
		}
	    }
	    message (D_ERROR, MSG_ERROR, _(" Cannot make the hardlink "));
	    return 0;
	}
    lp = (struct link *) g_malloc (sizeof (struct link) + strlen (src_name)
				   + strlen (dst_name) + 1);
    if (lp) {
    	char *lpdstname;
	lp->vfs = my_vfs;
	lp->ino = ino;
	lp->dev = dev;
	strcpy (lp->name, src_name);
	lpdstname = lp->name + strlen(lp->name) + 1;
	strcpy (lpdstname, dst_name);
	lp->next = linklist;
	linklist = lp;
    }
    return 0;
}

/*
 * Duplicate the contents of the symbolic link src_path in dst_path.
 * Try to make a stable symlink if the option "stable symlink" was
 * set in the file mask dialog.
 * If dst_path is an existing symlink it will be deleted silently
 * (upper levels take already care of existing files at dst_path).
 */
static FileProgressStatus
make_symlink (FileOpContext *ctx, const char *src_path, const char *dst_path)
{
    char link_target[MC_MAXPATHLEN];
    int len;
    FileProgressStatus return_status;
    struct stat sb;
    int dst_is_symlink;

    if (mc_lstat (dst_path, &sb) == 0 && S_ISLNK (sb.st_mode))
	dst_is_symlink = 1;
    else
	dst_is_symlink = 0;

  retry_src_readlink:
    len = mc_readlink (src_path, link_target, MC_MAXPATHLEN - 1);
    if (len < 0) {
	return_status =
	    file_error (_(" Cannot read source link \"%s\" \n %s "),
			src_path);
	if (return_status == FILE_RETRY)
	    goto retry_src_readlink;
	return return_status;
    }
    link_target[len] = 0;

    if (ctx->stable_symlinks)
	if (!vfs_file_is_local (src_path) || !vfs_file_is_local (dst_path)) {
	    message (D_ERROR, MSG_ERROR,
			_(" Cannot make stable symlinks across "
			  "non-local filesystems: \n\n"
			  " Option Stable Symlinks will be disabled "));
	    ctx->stable_symlinks = 0;
	}

    if (ctx->stable_symlinks && *link_target != PATH_SEP) {
	char *p, *q, *s;

	const char *r = strrchr (src_path, PATH_SEP);

	if (r) {
	    p = g_strndup (src_path, r - src_path + 1);
	    if (*dst_path == PATH_SEP)
		q = g_strdup (dst_path);
	    else
		q = g_strconcat (p, dst_path, (char *) NULL);
	    s = strrchr (q, PATH_SEP);
	    if (s) {
		s[1] = 0;
		s = g_strconcat (p, link_target, (char *) NULL);
		g_free (p);
		g_strlcpy (link_target, s, sizeof (link_target));
		g_free (s);
		s = diff_two_paths (q, link_target);
		if (s) {
		    g_strlcpy (link_target, s, sizeof (link_target));
		    g_free (s);
		}
	    } else
		g_free (p);
	    g_free (q);
	}
    }
  retry_dst_symlink:
    if (mc_symlink (link_target, dst_path) == 0)
	/* Success */
	return FILE_CONT;
    /*
     * if dst_exists, it is obvious that this had failed.
     * We can delete the old symlink and try again...
     */
    if (dst_is_symlink) {
	if (!mc_unlink (dst_path))
	    if (mc_symlink (link_target, dst_path) == 0)
		/* Success */
		return FILE_CONT;
    }
    return_status =
	file_error (_(" Cannot create target symlink \"%s\" \n %s "),
		    dst_path);
    if (return_status == FILE_RETRY)
	goto retry_dst_symlink;
    return return_status;
}

static int
progress_update_one (FileOpContext *ctx,
		     off_t *progress_count,
		     double *progress_bytes, off_t add, int is_toplevel_file)
{
    int ret;

    if (is_toplevel_file || ctx->progress_totals_computed) {
	(*progress_count)++;
	(*progress_bytes) += add;
    }

    /* Apply some heuristic here to not call the update stuff very often */
    ret =
	file_progress_show_count (ctx, *progress_count,
				  ctx->progress_count);
    if (ret != FILE_CONT)
	return ret;
    ret =
	file_progress_show_bytes (ctx, *progress_bytes,
				  ctx->progress_bytes);

    return ret;
}

/* Status of the destination file */
enum {
    DEST_NONE,			/* Not created */
    DEST_SHORT,			/* Created, not fully copied */
    DEST_FULL			/* Created, fully copied */
};

static FileProgressStatus
warn_same_file (const char *fmt, const char *a, const char *b)
{
    char *msg;
    int result = 0;
    msg = g_strdup_printf (fmt, a, b);
    result = query_dialog (MSG_ERROR, msg, D_ERROR, 2, _("&Skip"), _("&Abort"));
    g_free(msg);
    do_refresh ();
    if ( result ) { /* 1 == Abort */
        return FILE_ABORT;
    } else {
        return FILE_SKIP;
    }
}

FileProgressStatus
copy_file_file (FileOpContext *ctx, const char *src_path, const char *dst_path,
		int ask_overwrite, off_t *progress_count,
		double *progress_bytes, int is_toplevel_file)
{
    uid_t src_uid = (uid_t) - 1;
    gid_t src_gid = (gid_t) - 1;

    char *buf = NULL;
    int buf_size = BUF_8K;
    int src_desc, dest_desc = -1;
    int n_read, n_written;
    mode_t src_mode = 0;		/* The mode of the source file */
    struct stat sb, sb2;
    struct utimbuf utb;
    int dst_exists = 0, appending = 0;
    off_t n_read_total = 0, file_size = -1;
    FileProgressStatus return_status, temp_status;
    struct timeval tv_transfer_start;
    int dst_status = DEST_NONE;	/* 1 if the file is not fully copied */

    /* FIXME: We should not be using global variables! */
    ctx->do_reget = 0;
    return_status = FILE_RETRY;

    if (file_progress_show_source (ctx, src_path) == FILE_ABORT ||
	file_progress_show_target (ctx, dst_path) == FILE_ABORT)
	return FILE_ABORT;

    mc_refresh ();

    while (mc_stat (dst_path, &sb2) == 0) {
	if (S_ISDIR (sb2.st_mode)) {
	    return_status =
		file_error (_(" Cannot overwrite directory \"%s\" \n %s "),
			    dst_path);
	    if (return_status == FILE_RETRY)
		continue;
	    return return_status;
	}
	dst_exists = 1;
	break;
    }

    while ((*ctx->stat_func) (src_path, &sb)) {
	return_status =
	    file_error (_(" Cannot stat source file \"%s\" \n %s "),
			src_path);
	if (return_status != FILE_RETRY)
	    return return_status;
    }

    if (dst_exists) {
	/* Destination already exists */
        if (sb.st_dev == sb2.st_dev && sb.st_ino == sb2.st_ino)
            return warn_same_file (_(" `%s' \n and \n `%s' \n are the same file "),
                                src_path, dst_path);
	/* Should we replace destination? */
	if (ask_overwrite) {
	    ctx->do_reget = 0;
	    return_status = query_replace (ctx, dst_path, &sb, &sb2);
	    if (return_status != FILE_CONT)
		return return_status;
	}
    }

    if (!ctx->do_append) {
	/* Check the hardlinks */
	if (!ctx->follow_links && sb.st_nlink > 1 &&
	    check_hardlinks (src_path, dst_path, &sb) == 1) {
	    /* We have made a hardlink - no more processing is necessary */
	    return FILE_CONT;
	}

	if (S_ISLNK (sb.st_mode))
	    return make_symlink (ctx, src_path, dst_path);

	if (S_ISCHR (sb.st_mode) || S_ISBLK (sb.st_mode) || 
		S_ISFIFO (sb.st_mode) || S_ISNAM (sb.st_mode) ||
		S_ISSOCK (sb.st_mode)) {
	    while (mc_mknod (dst_path, sb.st_mode & ctx->umask_kill,
		    sb.st_rdev) < 0) {
		return_status = file_error (
		    _(" Cannot create special file \"%s\" \n %s "), dst_path);
		if (return_status == FILE_RETRY)
		    continue;
		return return_status;
	    }
	    /* Success */

	    while (ctx->preserve_uidgid
		   && mc_chown (dst_path, sb.st_uid, sb.st_gid)) {
		temp_status = file_error (
			_(" Cannot chown target file \"%s\" \n %s "), dst_path);
		if (temp_status == FILE_RETRY)
		    continue;
		return temp_status;
	    }
	    while (ctx->preserve &&
		   mc_chmod (dst_path, sb.st_mode & ctx->umask_kill)) {
		temp_status = file_error (
			_(" Cannot chmod target file \"%s\" \n %s "), dst_path);
		if (temp_status == FILE_RETRY)
		    continue;
		return temp_status;
	    }
	    return FILE_CONT;
	}
    }

    gettimeofday (&tv_transfer_start, (struct timezone *) NULL);

    while ((src_desc = mc_open (src_path, O_RDONLY | O_LINEAR)) < 0) {
	return_status = file_error (
		_(" Cannot open source file \"%s\" \n %s "), src_path);
	if (return_status == FILE_RETRY)
	    continue;
	ctx->do_append = 0;
	return return_status;
    }

    if (ctx->do_reget) {
	if (mc_lseek (src_desc, ctx->do_reget, SEEK_SET) != ctx->do_reget) {
	    message (D_ERROR, _("Warning"),
			_(" Reget failed, about to overwrite file "));
	    ctx->do_reget = ctx->do_append = 0;
	}
    }

    while (mc_fstat (src_desc, &sb)) {
	return_status = file_error (
		_(" Cannot fstat source file \"%s\" \n %s "), src_path);
	if (return_status == FILE_RETRY)
	    continue;
	ctx->do_append = 0;
	goto ret;
    }
    src_mode = sb.st_mode;
    src_uid = sb.st_uid;
    src_gid = sb.st_gid;
    utb.actime = sb.st_atime;
    utb.modtime = sb.st_mtime;
    file_size = sb.st_size;

    /* Create the new regular file with small permissions initially,
       do not create a security hole.  FIXME: You have security hole
       here, btw. Imagine copying to /tmp and symlink attack :-( */

    while ((dest_desc = mc_open (dst_path, O_WRONLY | (ctx->do_append ?
    	    O_APPEND : (O_CREAT | O_TRUNC)), 0600)) < 0) {
	return_status = file_error (
		_(" Cannot create target file \"%s\" \n %s "), dst_path);
	if (return_status == FILE_RETRY)
	    continue;
	ctx->do_append = 0;
	goto ret;
    }
    dst_status = DEST_SHORT;	/* file opened, but not fully copied */

    appending = ctx->do_append;
    ctx->do_append = 0;

    /* Find out the optimal buffer size.  */
    while (mc_fstat (dest_desc, &sb)) {
	return_status = file_error (
		_(" Cannot fstat target file \"%s\" \n %s "), dst_path);
	if (return_status == FILE_RETRY)
	    continue;
	goto ret;
    }
    buf = g_malloc (buf_size);

    ctx->eta_secs = 0.0;
    ctx->bps = 0;

    return_status = file_progress_show (ctx, 0, file_size);

    mc_refresh ();

    if (return_status != FILE_CONT)
	goto ret;

    {
	struct timeval tv_current, tv_last_update, tv_last_input;
	int secs, update_secs;
	long dt;
	const char *stalled_msg;

	tv_last_update = tv_transfer_start;

	for (;;) {
	    /* src_read */
	    if (mc_ctl (src_desc, VFS_CTL_IS_NOTREADY, 0))
		n_read = -1;
	    else
		while ((n_read = mc_read (src_desc, buf, buf_size)) < 0) {
		    return_status = file_error (
			_(" Cannot read source file \"%s\" \n %s "), src_path);
		    if (return_status == FILE_RETRY)
			continue;
		    goto ret;
		}
	    if (n_read == 0)
		break;

	    gettimeofday (&tv_current, NULL);

	    if (n_read > 0) {
		char *t = buf;
		n_read_total += n_read;

		/* Windows NT ftp servers report that files have no
		 * permissions: -------, so if we happen to have actually
		 * read something, we should fix the permissions.
		 */
		if (!(src_mode & (S_IRWXU | S_IRWXG | S_IRWXO)))
		    src_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		gettimeofday (&tv_last_input, NULL);

		/* dst_write */
		while ((n_written =
			mc_write (dest_desc, t, n_read)) < n_read) {
		    if (n_written > 0) {
			n_read -= n_written;
			t += n_written;
			continue;
		    }
		    return_status =
			file_error (_(" Cannot write target file \"%s\" \n %s "),
				dst_path);
		    if (return_status != FILE_RETRY)
			goto ret;
		}
	    }

	    /* 1. Update rotating dash after some time (hardcoded to 2 seconds) */
	    secs = (tv_current.tv_sec - tv_last_update.tv_sec);
	    if (secs > 2) {
		rotate_dash ();
		tv_last_update = tv_current;
	    }

	    /* 2. Check for a stalled condition */
	    update_secs = (tv_current.tv_sec - tv_last_input.tv_sec);
	    stalled_msg = "";
	    if (update_secs > 4) {
		stalled_msg = _("(stalled)");
	    }

	    /* 3. Compute ETA */
	    if (secs > 2) {
		dt = (tv_current.tv_sec - tv_transfer_start.tv_sec);

		if (n_read_total) {
		    ctx->eta_secs =
			((dt / (double) n_read_total) * file_size) - dt;
		    ctx->bps = n_read_total / ((dt < 1) ? 1 : dt);
		} else
		    ctx->eta_secs = 0.0;
	    }

	    /* 4. Compute BPS rate */
	    if (secs > 2) {
		ctx->bps_time =
		    (tv_current.tv_sec - tv_transfer_start.tv_sec);
		if (ctx->bps_time < 1)
		    ctx->bps_time = 1;
		ctx->bps = n_read_total / ctx->bps_time;
	    }

	    file_progress_set_stalled_label (ctx, stalled_msg);
	    return_status = file_progress_show_bytes (ctx, *progress_bytes +
		    n_read_total + ctx->do_reget, ctx->progress_bytes);
	    if (return_status == FILE_CONT) {
		return_status =
		    file_progress_show (ctx, n_read_total + ctx->do_reget, file_size);
	    }
	    mc_refresh ();
	    if (return_status != FILE_CONT)
		goto ret;
	}
    }

    dst_status = DEST_FULL;	/* copy successful, don't remove target file */

  ret:
    g_free (buf);

    while (src_desc != -1 && mc_close (src_desc) < 0) {
	temp_status = file_error (
		_(" Cannot close source file \"%s\" \n %s "), src_path);
	if (temp_status == FILE_RETRY)
	    continue;
	if (temp_status == FILE_ABORT)
	    return_status = temp_status;
	break;
    }

    while (dest_desc != -1 && mc_close (dest_desc) < 0) {
	temp_status = file_error (
		_(" Cannot close target file \"%s\" \n %s "), dst_path);
	if (temp_status == FILE_RETRY)
	    continue;
	return_status = temp_status;
	break;
    }

    if (dst_status == DEST_SHORT) {
	/* Remove short file */
	int result;
	result = query_dialog (_("Copy"),
		_("Incomplete file was retrieved. Keep it?"),
		D_ERROR, 2, _("&Delete"), _("&Keep"));
	if (!result)
	    mc_unlink (dst_path);
    } else if (dst_status == DEST_FULL) {
	/* Copy has succeeded */
	if (!appending && ctx->preserve_uidgid) {
	    while (mc_chown (dst_path, src_uid, src_gid)) {
		temp_status = file_error (
			_(" Cannot chown target file \"%s\" \n %s "), dst_path);
		if (temp_status == FILE_RETRY)
		    continue;
		return_status = temp_status;
		break;
	    }
	}

	if (!appending && ctx->preserve) {
	    while (mc_chmod (dst_path, (src_mode & ctx->umask_kill))) {
		temp_status = file_error (
			_(" Cannot chmod target file \"%s\" \n %s "), dst_path);
		if (temp_status != FILE_RETRY) {
		    return_status = temp_status;
		    break;
		}
	    }
	    mc_utime (dst_path, &utb);
	}
    }

    if (return_status == FILE_CONT)
	return_status =
	    progress_update_one (ctx, progress_count, progress_bytes,
				 file_size, is_toplevel_file);

    return return_status;
}

/*
 * I think these copy_*_* functions should have a return type.
 * anyway, this function *must* have two directories as arguments.
 */
/* FIXME: This function needs to check the return values of the
   function calls */
FileProgressStatus
copy_dir_dir (FileOpContext *ctx, const char *s, const char *_d, int toplevel,
	      int move_over, int delete, struct link *parent_dirs,
	      off_t *progress_count, double *progress_bytes)
{
    struct dirent *next;
    struct stat buf, cbuf;
    DIR *reading;
    char *path, *mdpath, *dest_file, *dest_dir;
    FileProgressStatus return_status = FILE_CONT;
    struct utimbuf utb;
    struct link *lp;
    char *d;

    /* First get the mode of the source dir */
  retry_src_stat:
    if ((*ctx->stat_func) (s, &cbuf)) {
	return_status =
	    file_error (_(" Cannot stat source directory \"%s\" \n %s "), s);
	if (return_status == FILE_RETRY)
	    goto retry_src_stat;
	return return_status;
    }

    if (is_in_linklist (dest_dirs, s, &cbuf)) {
	/* Don't copy a directory we created before (we don't want to copy 
	   infinitely if a directory is copied into itself) */
	/* FIXME: should there be an error message and FILE_SKIP? - Norbert */
	return FILE_CONT;
    }

    d = strutils_shell_unescape (_d);

/* Hmm, hardlink to directory??? - Norbert */
/* FIXME: In this step we should do something
   in case the destination already exist */
    /* Check the hardlinks */
    if (ctx->preserve && cbuf.st_nlink > 1
	&& check_hardlinks (s, d, &cbuf) == 1) {
	/* We have made a hardlink - no more processing is necessary */
	g_free(d);
	return return_status;
    }

    if (!S_ISDIR (cbuf.st_mode)) {
	return_status =
	    file_error (_(" Source \"%s\" is not a directory \n %s "), s);
	if (return_status == FILE_RETRY)
	    goto retry_src_stat;
	g_free(d);
	return return_status;
    }

    if (is_in_linklist (parent_dirs, s, &cbuf)) {
	/* we found a cyclic symbolic link */
	message (D_ERROR, MSG_ERROR,
		    _(" Cannot copy cyclic symbolic link \n `%s' "), s);
	g_free(d);
	return FILE_SKIP;
    }

    lp = g_new (struct link, 1);
    lp->vfs = vfs_get_class (s);
    lp->ino = cbuf.st_ino;
    lp->dev = cbuf.st_dev;
    lp->next = parent_dirs;
    parent_dirs = lp;

  retry_dst_stat:
    /* Now, check if the dest dir exists, if not, create it. */
    if (mc_stat (d, &buf)) {
	/* Here the dir doesn't exist : make it ! */

	if (move_over) {
	    if (mc_rename (s, d) == 0) {
		g_free (parent_dirs);
		g_free(d);
		return FILE_CONT;
	    }
	}
	dest_dir = g_strdup (d);
    } else {
	/*
	 * If the destination directory exists, we want to copy the whole
	 * directory, but we only want this to happen once.
	 *
	 * Escape sequences added to the * to compiler warnings.
	 * so, say /bla exists, if we copy /tmp/\* to /bla, we get /bla/tmp/\*
	 * or ( /bla doesn't exist )       /tmp/\* to /bla     ->  /bla/\*
	 */
	if (!S_ISDIR (buf.st_mode)) {
	    return_status = file_error(
		_(" Destination \"%s\" must be a directory \n %s "), d);
	    if (return_status == FILE_RETRY)
		goto retry_dst_stat;
	    g_free (parent_dirs);
	    g_free(d);
	    return return_status;
	}
	/* Dive into subdir if exists */
	if (toplevel && ctx->dive_into_subdirs) {
	    dest_dir = concat_dir_and_file (d, x_basename (s));
	} else {
	    dest_dir = g_strdup (d);
	    goto dont_mkdir;
	}
    }
    while (my_mkdir (dest_dir, (cbuf.st_mode & ctx->umask_kill) | S_IRWXU)) {
	return_status = file_error (
	    _(" Cannot create target directory \"%s\" \n %s "), dest_dir);
	if (return_status != FILE_RETRY)
	    goto ret;
    }

    lp = g_new (struct link, 1);
    mc_stat (dest_dir, &buf);
    lp->vfs = vfs_get_class (dest_dir);
    lp->ino = buf.st_ino;
    lp->dev = buf.st_dev;
    lp->next = dest_dirs;
    dest_dirs = lp;

    if (ctx->preserve_uidgid) {
	while (mc_chown (dest_dir, cbuf.st_uid, cbuf.st_gid)) {
	    return_status = file_error (
		_(" Cannot chown target directory \"%s\" \n %s "), dest_dir);
	    if (return_status != FILE_RETRY)
		goto ret;
	}
    }

  dont_mkdir:
    /* open the source dir for reading */
    if ((reading = mc_opendir (s)) == 0) {
	goto ret;
    }

    while ((next = mc_readdir (reading)) && return_status != FILE_ABORT) {
	/*
	 * Now, we don't want '.' and '..' to be created / copied at any time 
	 */
	if (!strcmp (next->d_name, "."))
	    continue;
	if (!strcmp (next->d_name, ".."))
	    continue;

	/* get the filename and add it to the src directory */
	path = concat_dir_and_file (s, next->d_name);

	(*ctx->stat_func) (path, &buf);
	if (S_ISDIR (buf.st_mode)) {
	    mdpath = concat_dir_and_file (dest_dir, next->d_name);
	    /*
	     * From here, we just intend to recursively copy subdirs, not
	     * the double functionality of copying different when the target
	     * dir already exists. So, we give the recursive call the flag 0
	     * meaning no toplevel.
	     */
	    return_status = copy_dir_dir (ctx, path, mdpath, 0, 0, delete,
				parent_dirs, progress_count, progress_bytes);
	    g_free (mdpath);
	} else {
	    dest_file = concat_dir_and_file (dest_dir, x_basename (path));
	    return_status = copy_file_file (ctx, path, dest_file, 1,
					    progress_count, progress_bytes, 0);
	    g_free (dest_file);
	}
	if (delete && return_status == FILE_CONT) {
	    if (ctx->erase_at_end) {
		static struct link *tail;
		lp = g_malloc (sizeof (struct link) + strlen (path));
		strcpy (lp->name, path);
		lp->st_mode = buf.st_mode;
		lp->next = 0;
		if (erase_list) {
		    tail->next = lp;
		    tail = lp;
		} else
		    erase_list = tail = lp;
	    } else {
		if (S_ISDIR (buf.st_mode)) {
		    return_status = erase_dir_iff_empty (ctx, path);
		} else
		    return_status = erase_file (ctx, path, 0, 0, 0);
	    }
	}
	g_free (path);
    }
    mc_closedir (reading);

    if (ctx->preserve) {
	mc_chmod (dest_dir, cbuf.st_mode & ctx->umask_kill);
	utb.actime = cbuf.st_atime;
	utb.modtime = cbuf.st_mtime;
	mc_utime (dest_dir, &utb);
    }

  ret:
    g_free (dest_dir);
    g_free (parent_dirs);
    g_free(d);
    return return_status;
}

/* }}} */

/* {{{ Move routines */

static FileProgressStatus
move_file_file (FileOpContext *ctx, const char *s, const char *d,
		off_t *progress_count, double *progress_bytes)
{
    struct stat src_stats, dst_stats;
    FileProgressStatus return_status = FILE_CONT;
    gboolean copy_done = FALSE;

    if (file_progress_show_source (ctx, s) == FILE_ABORT
	|| file_progress_show_target (ctx, d) == FILE_ABORT)
	return FILE_ABORT;

    mc_refresh ();

    while (mc_lstat (s, &src_stats) != 0) {
	/* Source doesn't exist */
	return_status =
	    file_error (_(" Cannot stat file \"%s\" \n %s "), s);
	if (return_status != FILE_RETRY)
	    return return_status;
    }

    if (mc_lstat (d, &dst_stats) == 0) {
	if (src_stats.st_dev == dst_stats.st_dev
            && src_stats.st_ino == dst_stats.st_ino)
            return warn_same_file (_(" `%s' \n and \n `%s' \n are the same file "), s, d);

	if (S_ISDIR (dst_stats.st_mode)) {
	    message (D_ERROR, MSG_ERROR,
			_(" Cannot overwrite directory `%s' "), d);
	    do_refresh ();
	    return FILE_SKIP;
	}

	if (confirm_overwrite) {
	    return_status = query_replace (ctx, d, &src_stats, &dst_stats);
	    if (return_status != FILE_CONT)
		return return_status;
	}
	/* Ok to overwrite */
    }

    if (!ctx->do_append) {
	if (S_ISLNK (src_stats.st_mode) && ctx->stable_symlinks) {
	    if ((return_status = make_symlink (ctx, s, d)) == FILE_CONT) {
		goto retry_src_remove;
	    } else
		return return_status;
	}

	if (mc_rename (s, d) == 0) {
	    return progress_update_one (ctx, progress_count,
	    				progress_bytes,
	    				src_stats.st_size, 1);
	}
    }
#if 0
/* Comparison to EXDEV seems not to work in nfs if you're moving from
   one nfs to the same, but on the server it is on two different
   filesystems. Then nfs returns EIO instead of EXDEV.
   Hope it will not hurt if we always in case of error try to copy/delete. */
    else
	errno = EXDEV;		/* Hack to copy (append) the file and then delete it */

    if (errno != EXDEV) {
	return_status =
	    files_error (_(" Cannot move file \"%s\" to \"%s\" \n %s "), s,
			 d);
	if (return_status == FILE_RETRY)
	    goto retry_rename;
	return return_status;
    }
#endif

    /* Failed because filesystem boundary -> copy the file instead */
    return_status =
	copy_file_file (ctx, s, d, 0, progress_count, progress_bytes, 1);
    if (return_status != FILE_CONT)
	return return_status;

    copy_done = TRUE;

    if ((return_status =
	 file_progress_show_source (ctx, NULL)) != FILE_CONT
	|| (return_status = file_progress_show (ctx, 0, 0)) != FILE_CONT)
	return return_status;

    mc_refresh ();

  retry_src_remove:
    if (mc_unlink (s)) {
	return_status =
	    file_error (_(" Cannot remove file \"%s\" \n %s "), s);
	if (return_status == FILE_RETRY)
	    goto retry_src_remove;
	return return_status;
    }

    if (!copy_done) {
	return_status = progress_update_one (ctx,
					     progress_count,
					     progress_bytes,
					     src_stats.st_size, 1);
    }

    return return_status;
}

FileProgressStatus
move_dir_dir (FileOpContext *ctx, const char *s, const char *d,
	      off_t *progress_count, double *progress_bytes)
{
    struct stat sbuf, dbuf, destbuf;
    struct link *lp;
    char *destdir;
    FileProgressStatus return_status;
    gboolean move_over = FALSE;
    gboolean dstat_ok;

    if (file_progress_show_source (ctx, s) == FILE_ABORT ||
	file_progress_show_target (ctx, d) == FILE_ABORT)
	return FILE_ABORT;

    mc_refresh ();

    mc_stat (s, &sbuf);
    dstat_ok = (mc_stat (d, &dbuf) == 0);

    if (dstat_ok && sbuf.st_dev == dbuf.st_dev && sbuf.st_ino == dbuf.st_ino)
        return warn_same_file (_(" `%s' \n and \n `%s' \n are the same directory "), s, d);

    if (!dstat_ok)
	destdir = g_strdup (d);	/* destination doesn't exist */
    else if (!ctx->dive_into_subdirs) {
	destdir = g_strdup (d);
	move_over = TRUE;
    } else
	destdir = concat_dir_and_file (d, x_basename (s));

    /* Check if the user inputted an existing dir */
  retry_dst_stat:
    if (!mc_stat (destdir, &destbuf)) {
	if (move_over) {
	    return_status = copy_dir_dir (ctx, s, destdir, 0, 1, 1, 0,
					  progress_count, progress_bytes);

	    if (return_status != FILE_CONT)
		goto ret;
	    goto oktoret;
	} else {
	    if (S_ISDIR (destbuf.st_mode))
		return_status =
		    file_error (_
				(" Cannot overwrite directory \"%s\" %s "),
				destdir);
	    else
		return_status =
		    file_error (_(" Cannot overwrite file \"%s\" %s "),
				destdir);
	    if (return_status == FILE_RETRY)
		goto retry_dst_stat;
	}
	g_free (destdir);
	return return_status;
    }

  retry_rename:
    if (mc_rename (s, destdir) == 0) {
	return_status = FILE_CONT;
	goto ret;
    }

    if (errno != EXDEV) {
	return_status =
	    files_error (_
			 (" Cannot move directory \"%s\" to \"%s\" \n %s "),
			 s, d);
	if (return_status == FILE_RETRY)
	    goto retry_rename;
	goto ret;
    }
    /* Failed because of filesystem boundary -> copy dir instead */
    return_status =
	copy_dir_dir (ctx, s, destdir, 0, 0, 1, 0, progress_count,
		      progress_bytes);

    if (return_status != FILE_CONT)
	goto ret;
  oktoret:
    if ((return_status =
	 file_progress_show_source (ctx, NULL)) != FILE_CONT
	|| (return_status = file_progress_show (ctx, 0, 0)) != FILE_CONT)
	goto ret;

    mc_refresh ();
    if (ctx->erase_at_end) {
	for (; erase_list && return_status != FILE_ABORT;) {
	    if (S_ISDIR (erase_list->st_mode)) {
		return_status =
		    erase_dir_iff_empty (ctx, erase_list->name);
	    } else
		return_status =
		    erase_file (ctx, erase_list->name, 0, 0, 0);
	    lp = erase_list;
	    erase_list = erase_list->next;
	    g_free (lp);
	}
    }
    erase_dir_iff_empty (ctx, s);

  ret:
    g_free (destdir);
    while (erase_list) {
	lp = erase_list;
	erase_list = erase_list->next;
	g_free (lp);
    }
    return return_status;
}

/* }}} */

/* {{{ Erase routines */
/* Don't update progress status if progress_count==NULL */
static FileProgressStatus
erase_file (FileOpContext *ctx, const char *s, off_t *progress_count,
	    double *progress_bytes, int is_toplevel_file)
{
    int return_status;
    struct stat buf;

    if (file_progress_show_deleting (ctx, s) == FILE_ABORT)
	return FILE_ABORT;
    mc_refresh ();

    if (progress_count && mc_lstat (s, &buf)) {
	/* ignore, most likely the mc_unlink fails, too */
	buf.st_size = 0;
    }

    while (mc_unlink (s)) {
	return_status =
	    file_error (_(" Cannot delete file \"%s\" \n %s "), s);
	if (return_status != FILE_RETRY)
	    return return_status;
    }

    if (progress_count)
	return progress_update_one (ctx, progress_count, progress_bytes,
				    buf.st_size, is_toplevel_file);
    else
	return FILE_CONT;
}

static FileProgressStatus
recursive_erase (FileOpContext *ctx, const char *s, off_t *progress_count,
		 double *progress_bytes)
{
    struct dirent *next;
    struct stat buf;
    DIR *reading;
    char *path;
    FileProgressStatus return_status = FILE_CONT;

    if (!strcmp (s, ".."))
	return FILE_RETRY;

    reading = mc_opendir (s);

    if (!reading)
	return FILE_RETRY;

    while ((next = mc_readdir (reading)) && return_status == FILE_CONT) {
	if (!strcmp (next->d_name, "."))
	    continue;
	if (!strcmp (next->d_name, ".."))
	    continue;
	path = concat_dir_and_file (s, next->d_name);
	if (mc_lstat (path, &buf)) {
	    g_free (path);
	    mc_closedir (reading);
	    return FILE_RETRY;
	}
	if (S_ISDIR (buf.st_mode))
	    return_status =
		(recursive_erase
		 (ctx, path, progress_count, progress_bytes)
		 != FILE_CONT) ? FILE_RETRY : FILE_CONT;
	else
	    return_status =
		erase_file (ctx, path, progress_count, progress_bytes, 0);
	g_free (path);
    }
    mc_closedir (reading);
    if (return_status != FILE_CONT)
	return return_status;
    if (file_progress_show_deleting (ctx, s) == FILE_ABORT)
	return FILE_ABORT;
    mc_refresh ();

    while (my_rmdir (s)) {
	return_status =
	    file_error (_(" Cannot remove directory \"%s\" \n %s "), s);
	if (return_status != FILE_RETRY)
	    return return_status;
    }

    return FILE_CONT;
}

/* Return -1 on error, 1 if there are no entries besides "." and ".." 
   in the directory path points to, 0 else. */
static int
check_dir_is_empty (const char *path)
{
    DIR *dir;
    struct dirent *d;
    int i;

    dir = mc_opendir (path);
    if (!dir)
	return -1;

    for (i = 1, d = mc_readdir (dir); d; d = mc_readdir (dir)) {
	if (d->d_name[0] == '.' && (d->d_name[1] == '\0' ||
				    (d->d_name[1] == '.'
				     && d->d_name[2] == '\0')))
	    continue;		/* "." or ".." */
	i = 0;
	break;
    }

    mc_closedir (dir);
    return i;
}

FileProgressStatus
erase_dir (FileOpContext *ctx, const char *s, off_t *progress_count,
	   double *progress_bytes)
{
    FileProgressStatus error;

    if (strcmp (s, "..") == 0)
	return FILE_SKIP;

    if (strcmp (s, ".") == 0)
	return FILE_SKIP;

    if (file_progress_show_deleting (ctx, s) == FILE_ABORT)
	return FILE_ABORT;
    mc_refresh ();

    /* The old way to detect a non empty directory was:
       error = my_rmdir (s);
       if (error && (errno == ENOTEMPTY || errno == EEXIST))){
       For the linux user space nfs server (nfs-server-2.2beta29-2)
       we would have to check also for EIO. I hope the new way is
       fool proof. (Norbert)
     */
    error = check_dir_is_empty (s);
    if (error == 0) {		/* not empty */
	error = query_recursive (ctx, s);
	if (error == FILE_CONT)
	    return recursive_erase (ctx, s, progress_count,
				    progress_bytes);
	else
	    return error;
    }

    while (my_rmdir (s) == -1) {
	error =
	    file_error (_(" Cannot remove directory \"%s\" \n %s "), s);
	if (error != FILE_RETRY)
	    return error;
    }

    return FILE_CONT;
}

static FileProgressStatus
erase_dir_iff_empty (FileOpContext *ctx, const char *s)
{
    FileProgressStatus error;

    if (strcmp (s, "..") == 0)
	return FILE_SKIP;

    if (strcmp (s, ".") == 0)
	return FILE_SKIP;

    if (file_progress_show_deleting (ctx, s) == FILE_ABORT)
	return FILE_ABORT;
    mc_refresh ();

    if (1 != check_dir_is_empty (s))	/* not empty or error */
	return FILE_CONT;

    while (my_rmdir (s)) {
	error =
	    file_error (_(" Cannot remove directory \"%s\" \n %s "), s);
	if (error != FILE_RETRY)
	    return error;
    }

    return FILE_CONT;
}

/* }}} */

/* {{{ Panel operate routines */

/*
 * Return currently selected entry name or the name of the first marked
 * entry if there is one.
 */
static char *
panel_get_file (WPanel *panel, struct stat *stat_buf)
{
    int i;

    if (get_current_type () == view_tree) {
	WTree *tree = (WTree *) get_panel_widget (get_current_index ());
	char *tree_name = tree_selected_name (tree);

	mc_stat (tree_name, stat_buf);
	return tree_name;
    }

    if (panel->marked) {
	for (i = 0; i < panel->count; i++)
	    if (panel->dir.list[i].f.marked) {
		*stat_buf = panel->dir.list[i].st;
		return panel->dir.list[i].fname;
	    }
    } else {
	*stat_buf = panel->dir.list[panel->selected].st;
	return panel->dir.list[panel->selected].fname;
    }
    g_assert_not_reached ();
    return NULL;
}


ComputeDirSizeUI *
compute_dir_size_create_ui (void)
{
    ComputeDirSizeUI *ui;

    char *b_name = N_("&Abort");

#ifdef ENABLE_NLS
    b_name = _(b_name);
#endif

    ui = g_new (ComputeDirSizeUI, 1);

    ui->dlg = create_dlg (0, 0, 8, COLS/2, dialog_colors, NULL,
			    NULL, _("Directory scanning"), DLG_CENTER);
    ui->dirname = label_new (3, 3, "");
    add_widget (ui->dlg, ui->dirname);

    add_widget (ui->dlg,
		button_new (5, (ui->dlg->cols - strlen (b_name))/2,
		FILE_ABORT, NORMAL_BUTTON, b_name, NULL));

    /* We will manage the dialog without any help,
       that's why we have to call init_dlg */
    init_dlg (ui->dlg);

    return ui;
}

void
compute_dir_size_destroy_ui (ComputeDirSizeUI *ui)
{
    if (ui != NULL) {
	/* schedule to update passive panel */
	other_panel->dirty = 1;

	/* close and destroy dialog */
	dlg_run_done (ui->dlg);
	destroy_dlg (ui->dlg);
	g_free (ui);
    }
}

FileProgressStatus
compute_dir_size_update_ui (const void *ui, const char *dirname)
{
    const ComputeDirSizeUI *this = (const ComputeDirSizeUI *) ui;
    int c;
    Gpm_Event event;

    if (ui == NULL)
	return FILE_CONT;

    label_set_text (this->dirname, name_trunc (dirname, this->dlg->cols - 6));

    event.x = -1; /* Don't show the GPM cursor */
    c = get_event (&event, 0, 0);
    if (c == EV_NONE)
	return FILE_CONT;

    /* Reinitialize to avoid old values after events other than
       selecting a button */
    this->dlg->ret_value = FILE_CONT;

    dlg_process_event (this->dlg, c, &event);

    switch (this->dlg->ret_value) {
    case B_CANCEL:
    case FILE_ABORT:
	return FILE_ABORT;
    default:
	return FILE_CONT;
    }
}

/**
 * compute_dir_size:
 *
 * Computes the number of bytes used by the files in a directory
 */
FileProgressStatus
compute_dir_size (const char *dirname, const void *ui,
		    compute_dir_size_callback cback,
		    off_t *ret_marked, double *ret_total)
{
    DIR *dir;
    struct dirent *dirent;
    FileProgressStatus ret = FILE_CONT;

    dir = mc_opendir (dirname);

    if (dir == NULL)
	return ret;

    while ((dirent = mc_readdir (dir)) != NULL) {
	char *fullname;
	int res;
	struct stat s;

	ret = (cback != NULL) ? cback (ui, dirname) : FILE_CONT;

	if (ret != FILE_CONT)
	    break;

	if (strcmp (dirent->d_name, ".") == 0)
	    continue;
	if (strcmp (dirent->d_name, "..") == 0)
	    continue;

	fullname = concat_dir_and_file (dirname, dirent->d_name);
	res = mc_lstat (fullname, &s);

	if (res != 0) {
	    g_free (fullname);
	    continue;
	}

	if (S_ISDIR (s.st_mode)) {
	    off_t subdir_count = 0;
	    double subdir_bytes = 0;

	    ret = compute_dir_size (fullname, ui, cback, &subdir_count, &subdir_bytes);

	    if (ret != FILE_CONT) {
		g_free (fullname);
		break;
	    }

	    *ret_marked += subdir_count;
	    *ret_total += subdir_bytes;
	} else {
	    (*ret_marked)++;
	    *ret_total += s.st_size;
	}

	g_free (fullname);
    }

    mc_closedir (dir);

    return ret;
}

/**
 * panel_compute_totals:
 *
 * compute the number of files and the number of bytes
 * used up by the whole selection, recursing directories
 * as required.  In addition, it checks to see if it will
 * overwrite any files by doing the copy.
 */
static FileProgressStatus
panel_compute_totals (WPanel *panel, const void *ui,
			compute_dir_size_callback cback,
			off_t *ret_marked, double *ret_total)
{
    int i;

    *ret_marked = 0;
    *ret_total = 0.0;

    for (i = 0; i < panel->count; i++) {
	struct stat *s;

	if (!panel->dir.list[i].f.marked)
	    continue;

	s = &panel->dir.list[i].st;

	if (S_ISDIR (s->st_mode)) {
	    char *dir_name;
	    off_t subdir_count = 0;
	    double subdir_bytes = 0;
	    FileProgressStatus status;

	    dir_name =
		concat_dir_and_file (panel->cwd, panel->dir.list[i].fname);

	    status  = compute_dir_size (dir_name, ui, cback,
					&subdir_count, &subdir_bytes);
	    g_free (dir_name);

	    if (status != FILE_CONT)
		return FILE_ABORT;

	    *ret_marked += subdir_count;
	    *ret_total += subdir_bytes;
	} else {
	    (*ret_marked)++;
	    *ret_total += s->st_size;
	}
    }

    return FILE_CONT;
}

/*
 * This array introduced to avoid translation problems. The former (op_names)
 * is assumed to be nouns, suitable in dialog box titles; this one should
 * contain whatever is used in prompt itself (i.e. in russian, it's verb).
 * Notice first symbol - it is to fool gettext and force these strings to
 * be different for it. First symbol is skipped while building a prompt.
 * (I don't use spaces around the words, because someday they could be
 * dropped, when widgets get smarter)
 */
static const char *op_names1[] = { N_("1Copy"), N_("1Move"), N_("1Delete") };
#define	FMD_XLEN 64

int fmd_xlen = FMD_XLEN;

/*
 * These are formats for building a prompt. Parts encoded as follows:
 * %o - operation from op_names1
 * %f - file/files or files/directories, as appropriate
 * %m - "with source mask" or question mark for delete
 * %s - source name (truncated)
 * %d - number of marked files
 * %e - "to:" or question mark for delete
 * 
 * xgettext:no-c-format */
static const char *one_format = N_("%o %f \"%s\"%m");
/* xgettext:no-c-format */
static const char *many_format = N_("%o %d %f%m");
static const char *prompt_parts[] = {
    N_("file"), N_("files"), N_("directory"), N_("directories"),
    N_("files/directories"), N_(" with source mask:"), N_(" to:")
};

/*
 * Generate user prompt for panel operation.
 * single_source is the name if the source entry or NULL for multiple
 * entries.
 * src_stat is only used when single_source is not NULL.
 */
static void
panel_operate_generate_prompt (const WPanel *panel, const int operation,
			       const char *single_source,
			       const struct stat *src_stat)
{
    register const char *sp, *cp;
    register int i;
    char format_string[BUF_MEDIUM];
    char *dp = format_string;

#ifdef ENABLE_NLS
    static int i18n_flag = 0;
    if (!i18n_flag) {
	fmd_init_i18n (FALSE);	/* to get proper fmd_xlen */

	for (i = sizeof (op_names1) / sizeof (op_names1[0]); i--;)
	    op_names1[i] = _(op_names1[i]);

	for (i = sizeof (prompt_parts) / sizeof (prompt_parts[0]); i--;)
	    prompt_parts[i] = _(prompt_parts[i]);

	one_format = _(one_format);
	many_format = _(many_format);
	i18n_flag = 1;
    }
#endif				/* ENABLE_NLS */

    sp = single_source ? one_format : many_format;

    while (*sp) {
	switch (*sp) {
	case '%':
	    cp = NULL;
	    switch (sp[1]) {
	    case 'o':
		cp = op_names1[operation] + 1;
		break;
	    case 'm':
		cp = operation == OP_DELETE ? "?" : prompt_parts[5];
		break;
	    case 'e':
		cp = operation == OP_DELETE ? "?" : prompt_parts[6];
		break;
	    case 'f':
		if (single_source) {
		    cp = S_ISDIR (src_stat->
				  st_mode) ? prompt_parts[2] :
			prompt_parts[0];
		} else {
		    cp = (panel->marked == panel->dirs_marked)
			? prompt_parts[3]
			: (panel->dirs_marked ? prompt_parts[4]
			   : prompt_parts[1]);
		}
		break;
	    default:
		*dp++ = *sp++;
	    }
	    if (cp) {
		sp += 2;
		while (*cp)
		    *dp++ = *cp++;
	    }
	    break;
	default:
	    *dp++ = *sp++;
	}
    }
    *dp = '\0';

    if (single_source) {
        i = fmd_xlen - str_term_width1 (format_string) - 4;
	g_snprintf (cmd_buf, sizeof (cmd_buf), format_string,
		    str_trunc (single_source, i));
    } else {
	g_snprintf (cmd_buf, sizeof (cmd_buf), format_string,
		    panel->marked);
        i = str_term_width1 (cmd_buf) + 6 - fmd_xlen;
	if (i > 0) {
	    fmd_xlen += i;
	    fmd_init_i18n (TRUE);	/* to recalculate positions of child widgets */
	}
    }
}

/**
 * panel_operate:
 *
 * Performs one of the operations on the selection on the source_panel
 * (copy, delete, move).  
 *
 * Returns 1 if did change the directory
 * structure, Returns 0 if user aborted
 *
 * force_single forces operation on the current entry and affects
 * default destination.  Current filename is used as default.
 */
int
panel_operate (void *source_panel, FileOperation operation,
	       int force_single)
{
    WPanel *panel = source_panel;
#ifdef WITH_FULL_PATHS
    char *source_with_path = NULL;
#else
#   define source_with_path source
#endif				/* !WITH_FULL_PATHS */
    char *source = NULL;
    char *dest = NULL;
    char *temp = NULL;
    char *save_cwd = NULL, *save_dest = NULL;
    int single_entry = (get_current_type () == view_tree)
	|| (panel->marked <= 1) || force_single;
    struct stat src_stat, dst_stat;
    int i;
    FileProgressStatus value;
    FileOpContext *ctx;

    off_t count = 0;
    double bytes = 0;

    int dst_result;
    int do_bg = 0;		/* do background operation? */

    free_linklist (&linklist);
    free_linklist (&dest_dirs);

    if (single_entry) {
	if (force_single) {
	    source = selection (panel)->fname;
	    src_stat = selection (panel)->st;
	} else {
	    source = panel_get_file (panel, &src_stat);
	}

	if (!strcmp (source, "..")) {
	    message (D_ERROR, MSG_ERROR, _(" Cannot operate on \"..\"! "));
	    return 0;
	}
    }

    /* Generate confirmation prompt */
    panel_operate_generate_prompt (panel, operation, source, &src_stat);

    ctx = file_op_context_new (operation);

    /* Show confirmation dialog */
    if (operation == OP_DELETE && confirm_delete) {
	if (safe_delete)
	    query_set_sel (1);

	i = query_dialog (_(op_names[operation]), cmd_buf, D_ERROR, 2,
			  _("&Yes"), _("&No"));

	if (i != 0) {
	    file_op_context_destroy (ctx);
	    return 0;
	}
    } else if (operation != OP_DELETE) {
	char *dest_dir;
	char *dest_dir_;

	/* Forced single operations default to the original name */
	if (force_single)
	    dest_dir = source;
	else if (get_other_type () == view_listing)
	    dest_dir = other_panel->cwd;
	else
	    dest_dir = panel->cwd;
	/*
	 * Add trailing backslash only when do non-local ops.
	 * It saves user from occasional file renames (when destination
	 * dir is deleted)
	 */
	if (!force_single
	 && dest_dir[0]
	 && dest_dir[strlen(dest_dir)-1] != PATH_SEP) {
	    /* add trailing separator */
	    dest_dir_ = g_strconcat (dest_dir, PATH_SEP_STR, (char*)0);
	} else {
	    /* just copy */
	    dest_dir_ = g_strdup (dest_dir);
	}
	if (!dest_dir_) {
	    file_op_context_destroy (ctx);
	    return 0;
	}

	dest =
	    file_mask_dialog (ctx, operation, cmd_buf, dest_dir_,
			      single_entry, &do_bg);
	g_free(dest_dir_);

	if (!dest || !dest[0]) {
	    file_op_context_destroy (ctx);
	    g_free (dest);
	    return 0;
	}
    }
#ifdef WITH_BACKGROUND
    /* Did the user select to do a background operation? */
    if (do_bg) {
	int v;

	v = do_background (ctx,
			   g_strconcat (op_names[operation], ": ",
					panel->cwd, NULL));
	if (v == -1) {
	    message (D_ERROR, MSG_ERROR,
		     _(" Sorry, I could not put the job in background "));
	}

	/* If we are the parent */
	if (v == 1) {
	    mc_setctl (panel->cwd, VFS_SETCTL_FORGET, NULL);
	    mc_setctl (dest, VFS_SETCTL_FORGET, NULL);
/*  	    file_op_context_destroy (ctx); */
	    return 0;
	}
    }
#endif				/* WITH_BACKGROUND */

    /* Initialize things */
    /* We do not want to trash cache every time file is
       created/touched. However, this will make our cache contain
       invalid data. */
    if (dest) {
	if (mc_setctl (dest, VFS_SETCTL_STALE_DATA, (void *) 1))
	    save_dest = g_strdup (dest);
    }
    if (panel->cwd) {
	if (mc_setctl (panel->cwd, VFS_SETCTL_STALE_DATA, (void *) 1))
	    save_cwd = g_strdup (panel->cwd);
    }

    /* Now, let's do the job */

    if (do_bg)
	ctx->ui = NULL;
    else
	file_op_context_create_ui (ctx, 1);

    /* This code is only called by the tree and panel code */
    if (single_entry) {
	/* We now have ETA in all cases */

	/* One file: FIXME mc_chdir will take user out of any vfs */
	if (operation != OP_COPY && get_current_type () == view_tree)
	    mc_chdir (PATH_SEP_STR);

	/* The source and src_stat variables have been initialized before */
#ifdef WITH_FULL_PATHS
	source_with_path = concat_dir_and_file (panel->cwd, source);
#endif				/* WITH_FULL_PATHS */

	if (operation == OP_DELETE) {
	    if (S_ISDIR (src_stat.st_mode))
		value = erase_dir (ctx, source_with_path, &count, &bytes);
	    else
		value =
		    erase_file (ctx, source_with_path, &count, &bytes, 1);
	} else {
	    temp = transform_source (ctx, source_with_path);
	    if (temp == NULL) {
		value = transform_error;
	    } else {
		char *repl_dest = mc_search_prepare_replace_str2 (ctx->search_handle, dest);
		char *temp2 = concat_dir_and_file (repl_dest, temp);
		g_free (repl_dest);
		g_free (temp);
		g_free(dest);
		dest = temp2;
		
		switch (operation) {
		case OP_COPY:
		    /*
		     * we use file_mask_op_follow_links only with OP_COPY,
		     */
		    (*ctx->stat_func) (source_with_path, &src_stat);

		    if (S_ISDIR (src_stat.st_mode)) {
			value =
			    copy_dir_dir (ctx, source_with_path, dest, 1,
					  0, 0, 0, &count, &bytes);
		    } else {
			value =
			    copy_file_file (ctx, source_with_path, dest, 1,
					    &count, &bytes, 1);
                    }
		    break;

		case OP_MOVE:
		    if (S_ISDIR (src_stat.st_mode))
			value =
			    move_dir_dir (ctx, source_with_path, dest,
					  &count, &bytes);
		    else
			value =
			    move_file_file (ctx, source_with_path, dest,
					    &count, &bytes);
		    break;

		default:
		    /* Unknown file operation */
		    abort ();
		}
	    }
	}			/* Copy or move operation */

	if ((value == FILE_CONT) && !force_single)
	    unmark_files (panel);
    } else {
	/* Many files */
	/* Check destination for copy or move operation */
	if (operation != OP_DELETE) {
	  retry_many_dst_stat:
	    dst_result = mc_stat (dest, &dst_stat);
	    if (dst_result == 0 && !S_ISDIR (dst_stat.st_mode)) {
		if (file_error
		    (_(" Destination \"%s\" must be a directory \n %s "),
		     dest) == FILE_RETRY)
		    goto retry_many_dst_stat;
		goto clean_up;
	    }
	}

	/* Initialize variables for progress bars */
	if (operation != OP_MOVE && verbose && file_op_compute_totals) {
	    ComputeDirSizeUI *ui;
	    FileProgressStatus status;

	    ui = compute_dir_size_create_ui ();
	    status = panel_compute_totals (panel,
				ui, compute_dir_size_update_ui,
				&ctx->progress_count, &ctx->progress_bytes);
	    compute_dir_size_destroy_ui (ui);

	    if (status != FILE_CONT)
		goto clean_up;

	    ctx->progress_totals_computed = 1;
	} else {
	    ctx->progress_totals_computed = 0;
	    ctx->progress_count = panel->marked;
	    ctx->progress_bytes = panel->total;
	}

	/* Loop for every file, perform the actual copy operation */
	for (i = 0; i < panel->count; i++) {
	    if (!panel->dir.list[i].f.marked)
		continue;	/* Skip the unmarked ones */

	    source = panel->dir.list[i].fname;
	    src_stat = panel->dir.list[i].st;

#ifdef WITH_FULL_PATHS
	    g_free (source_with_path);
	    source_with_path = concat_dir_and_file (panel->cwd, source);
#endif				/* WITH_FULL_PATHS */

	    if (operation == OP_DELETE) {
		if (S_ISDIR (src_stat.st_mode))
		    value =
			erase_dir (ctx, source_with_path, &count, &bytes);
		else
		    value =
			erase_file (ctx, source_with_path, &count, &bytes,
				    1);
	    } else {
		temp = transform_source (ctx, source_with_path);
		if (temp == NULL)
		    value = transform_error;
		else {
		    char *temp3;
		    char *repl_dest = mc_search_prepare_replace_str2 (ctx->search_handle, dest);
		    char *temp2 = concat_dir_and_file (repl_dest, temp);
		    g_free(repl_dest);

		    g_free(temp);
		    temp3 = source_with_path;
		    source_with_path = strutils_shell_unescape(source_with_path);
		    g_free(temp3);
		    temp3 = temp2;
		    temp2 = strutils_shell_unescape(temp2);
		    g_free(temp3);

		    switch (operation) {
		    case OP_COPY:
			/*
			 * we use file_mask_op_follow_links only with OP_COPY
			 */
			(*ctx->stat_func) (source_with_path, &src_stat);
			if (S_ISDIR (src_stat.st_mode))
			    value =
				copy_dir_dir (ctx, source_with_path, temp2,
					      1, 0, 0, 0, &count, &bytes);
			else
			    value =
				copy_file_file (ctx, source_with_path,
						temp2, 1, &count, &bytes,
						1);
			free_linklist (&dest_dirs);
			break;

		    case OP_MOVE:
			if (S_ISDIR (src_stat.st_mode))
			    value =
				move_dir_dir (ctx, source_with_path, temp2,
					      &count, &bytes);
			else
			    value =
				move_file_file (ctx, source_with_path,
						temp2, &count, &bytes);
			break;

		    default:
			/* Unknown file operation */
			abort ();
		    }
		    g_free (temp2);
		}
	    }			/* Copy or move operation */

	    if (value == FILE_ABORT)
		goto clean_up;

	    if (value == FILE_CONT)
		do_file_mark (panel, i, 0);

	    if (file_progress_show_count (ctx, count, ctx->progress_count)
		== FILE_ABORT)
		goto clean_up;

	    if (verbose
		&& file_progress_show_bytes (ctx, bytes,
					     ctx->progress_bytes) ==
		FILE_ABORT)
		goto clean_up;

	    if (operation != OP_DELETE && verbose
		&& file_progress_show (ctx, 0, 0) == FILE_ABORT)
		goto clean_up;

	    mc_refresh ();
	}			/* Loop for every file */
    }				/* Many entries */
  clean_up:
    /* Clean up */

    if (save_cwd) {
	mc_setctl (save_cwd, VFS_SETCTL_STALE_DATA, NULL);
	g_free (save_cwd);
    }
    if (save_dest) {
	mc_setctl (save_dest, VFS_SETCTL_STALE_DATA, NULL);
	g_free (save_dest);
    }

    free_linklist (&linklist);
    free_linklist (&dest_dirs);
#ifdef WITH_FULL_PATHS
    g_free (source_with_path);
#endif				/* WITH_FULL_PATHS */
    g_free (dest);
    g_free (ctx->dest_mask);
    ctx->dest_mask = NULL;
#ifdef WITH_BACKGROUND
    /* Let our parent know we are saying bye bye */
    if (we_are_background) {
	vfs_shut ();
	_exit (0);
    }
#endif				/* WITH_BACKGROUND */

    file_op_context_destroy (ctx);
    return 1;
}

/* }}} */

/* {{{ Query/status report routines */

static FileProgressStatus
real_do_file_error (enum OperationMode mode, const char *error)
{
    int result;
    const char *msg;

    msg = mode == Foreground ? MSG_ERROR : _(" Background process error ");
    result =
	query_dialog (msg, error, D_ERROR, 3, _("&Skip"), _("&Retry"),
		      _("&Abort"));

    switch (result) {
    case 0:
	do_refresh ();
	return FILE_SKIP;

    case 1:
	do_refresh ();
	return FILE_RETRY;

    case 2:
    default:
	return FILE_ABORT;
    }
}

/* Report error with one file */
FileProgressStatus
file_error (const char *format, const char *file)
{
    g_snprintf (cmd_buf, sizeof (cmd_buf), format,
		path_trunc (file, 30), unix_error_string (errno));

    return do_file_error (cmd_buf);
}

/* Report error with two files */
static FileProgressStatus
files_error (const char *format, const char *file1, const char *file2)
{
    char *nfile1 = g_strdup (path_trunc (file1, 15));
    char *nfile2 = g_strdup (path_trunc (file2, 15));

    g_snprintf (cmd_buf, sizeof (cmd_buf), format, nfile1, nfile2,
		unix_error_string (errno));

    g_free (nfile1);
    g_free (nfile2);
    
    return do_file_error (cmd_buf);
}

static FileProgressStatus
real_query_recursive (FileOpContext *ctx, enum OperationMode mode, const char *s)
{
    gchar *text;

    if (ctx->recursive_result < RECURSIVE_ALWAYS) {
	const char *msg =
	    mode ==
	    Foreground ?
	    _("\n   Directory not empty.   \n"
	      "   Delete it recursively? ")
	    : _("\n   Background process: Directory not empty \n"
		"   Delete it recursively? ");
	text = g_strconcat (_(" Delete: "), path_trunc (s, 30), " ", (char *) NULL);

	if (safe_delete)
	    query_set_sel (1);
	ctx->recursive_result = query_dialog (text, msg, D_ERROR, 5,
					      _("&Yes"), _("&No"),
					      _("A&ll"), _("Non&e"),
					      _("&Abort"));

	if (ctx->recursive_result != RECURSIVE_ABORT)
	    do_refresh ();
	g_free (text);
    }

    switch (ctx->recursive_result) {
    case RECURSIVE_YES:
    case RECURSIVE_ALWAYS:
	return FILE_CONT;

    case RECURSIVE_NO:
    case RECURSIVE_NEVER:
	return FILE_SKIP;

    case RECURSIVE_ABORT:

    default:
	return FILE_ABORT;
    }
}

#ifdef WITH_BACKGROUND
static FileProgressStatus
do_file_error (const char *str)
{
    union {
	void *p;
	FileProgressStatus (*f) (enum OperationMode, const char *);
    } pntr;
    pntr.f = real_do_file_error;

    if (we_are_background)
	return parent_call (pntr.p, NULL, 1, strlen (str),
			    str);
    else
	return real_do_file_error (Foreground, str);
}

static FileProgressStatus
query_recursive (FileOpContext *ctx, const char *s)
{
    union {
	void *p;
	FileProgressStatus (*f) (FileOpContext *, enum OperationMode, const char *);
    } pntr;
    pntr.f = real_query_recursive;

    if (we_are_background)
	return parent_call (pntr.p, ctx, 1, strlen (s), s);
    else
	return real_query_recursive (ctx, Foreground, s);
}

static FileProgressStatus
query_replace (FileOpContext *ctx, const char *destname, struct stat *_s_stat,
	       struct stat *_d_stat)
{
    union {
	void *p;
	FileProgressStatus (*f) (FileOpContext *, enum OperationMode, const char *,
				  struct stat *, struct stat *);
    } pntr;
    pntr.f = file_progress_real_query_replace;

    if (we_are_background)
	return parent_call (pntr.p,
			    ctx,
			    3,
			    strlen (destname), destname,
			    sizeof (struct stat), _s_stat,
			    sizeof (struct stat), _d_stat);
    else
	return file_progress_real_query_replace (ctx, Foreground, destname,
						 _s_stat, _d_stat);
}

#else
static FileProgressStatus
do_file_error (const char *str)
{
    return real_do_file_error (Foreground, str);
}

static FileProgressStatus
query_recursive (FileOpContext *ctx, const char *s)
{
    return real_query_recursive (ctx, Foreground, s);
}

static FileProgressStatus
query_replace (FileOpContext *ctx, const char *destname, struct stat *_s_stat,
	       struct stat *_d_stat)
{
    return file_progress_real_query_replace (ctx, Foreground, destname,
					     _s_stat, _d_stat);
}

#endif				/* !WITH_BACKGROUND */

/*
  Cause emacs to enter folding mode for this file:
  Local variables:
  end:
*/
