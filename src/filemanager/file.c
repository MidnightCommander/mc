/*
   File management.

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2011, 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1995
   Fred Leeflang, 1994, 1995
   Miguel de Icaza, 1994, 1995, 1996
   Jakub Jelinek, 1995, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Andrew Borodin <aborodin@vmail.ru>, 2011, 2012, 2013

   The copy code was based in GNU's cp, and was written by:
   Torbjorn Granlund, David MacKenzie, and Jim Meyering.

   The move code was based in GNU's mv, and was written by:
   Mike Parker and David MacKenzie.

   Janne Kukonlehto added much error recovery to them for being used
   in an interactive program.

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

/*
 * Please note that all dialogs used here must be safe for background
 * operations.
 */

/** \file src/filemanager/file.c
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

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/search.h"
#include "lib/strescape.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/setup.h"
#ifdef ENABLE_BACKGROUND
#include "src/background.h"     /* do_background() */
#endif

#include "layout.h"             /* rotate_dash() */

/* Needed for current_panel, other_panel and WTree */
#include "dir.h"
#include "filegui.h"
#include "filenot.h"
#include "tree.h"
#include "midnight.h"           /* current_panel */

#include "file.h"

/* }}} */

/*** global variables ****************************************************************************/

/* TRANSLATORS: no need to translate 'DialogTitle', it's just a context prefix  */
const char *op_names[3] = {
    N_("DialogTitle|Copy"),
    N_("DialogTitle|Move"),
    N_("DialogTitle|Delete")
};

/*** file scope macro definitions ****************************************************************/

/* Hack: the vfs code should not rely on this */
#define WITH_FULL_PATHS 1

#define FILEOP_UPDATE_INTERVAL 2
#define FILEOP_STALLING_INTERVAL 4

/*** file scope type declarations ****************************************************************/

/* This is a hard link cache */
struct link
{
    const struct vfs_class *vfs;
    dev_t dev;
    ino_t ino;
    short linkcount;
    mode_t st_mode;
    vfs_path_t *src_vpath;
    vfs_path_t *dst_vpath;
};

/* Status of the destination file */
typedef enum
{
    DEST_NONE = 0,              /* Not created */
    DEST_SHORT = 1,             /* Created, not fully copied */
    DEST_FULL = 2               /* Created, fully copied */
} dest_status_t;

/*
 * This array introduced to avoid translation problems. The former (op_names)
 * is assumed to be nouns, suitable in dialog box titles; this one should
 * contain whatever is used in prompt itself (i.e. in russian, it's verb).
 * (I don't use spaces around the words, because someday they could be
 * dropped, when widgets get smarter)
 */

/* TRANSLATORS: no need to translate 'FileOperation', it's just a context prefix  */
static const char *op_names1[] = {
    N_("FileOperation|Copy"),
    N_("FileOperation|Move"),
    N_("FileOperation|Delete")
};

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
    N_("file"),
    N_("files"),
    N_("directory"),
    N_("directories"),
    N_("files/directories"),
    /* TRANSLATORS: keep leading space here to split words in Copy/Move dialog */
    N_(" with source mask:"),
    N_("to:")
};

static const char *question_format = N_("%s?");

/*** file scope variables ************************************************************************/

/* the hard link cache */
static GSList *linklist = NULL;

/* the files-to-be-erased list */
static GSList *erase_list = NULL;

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
static GSList *dest_dirs = NULL;

static FileProgressStatus transform_error = FILE_CONT;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
transform_source (FileOpContext * ctx, const char *source)
{
    char *s, *q;
    char *fnsource;

    s = g_strdup (source);

    /* We remove \n from the filename since regex routines would use \n as an anchor */
    /* this is just to be allowed to maniupulate file names with \n on it */
    for (q = s; *q != '\0'; q++)
        if (*q == '\n')
            *q = ' ';

    fnsource = (char *) x_basename (s);

    if (mc_search_run (ctx->search_handle, fnsource, 0, strlen (fnsource), NULL))
    {
        q = mc_search_prepare_replace_str2 (ctx->search_handle, ctx->dest_mask);
        if (ctx->search_handle->error != MC_SEARCH_E_OK)
        {
            message (D_ERROR, MSG_ERROR, "%s", ctx->search_handle->error_str);
            q = NULL;
            transform_error = FILE_ABORT;
        }
    }
    else
    {
        q = NULL;
        transform_error = FILE_SKIP;
    }

    g_free (s);
    return q;
}

/* --------------------------------------------------------------------------------------------- */

static void
free_link (void *data)
{
    struct link *lp = (struct link *) data;

    vfs_path_free (lp->src_vpath);
    vfs_path_free (lp->dst_vpath);
    g_free (lp);
}

/* --------------------------------------------------------------------------------------------- */

static void *
free_linklist (GSList * lp)
{
    g_slist_foreach (lp, (GFunc) free_link, NULL);
    g_slist_free (lp);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_in_linklist (const GSList * lp, const vfs_path_t * vpath, const struct stat *sb)
{
    const struct vfs_class *class;
    ino_t ino = sb->st_ino;
    dev_t dev = sb->st_dev;

    class = vfs_path_get_last_path_vfs (vpath);

    for (; lp != NULL; lp = g_slist_next (lp))
    {
        const struct link *lnk = (const struct link *) lp->data;

        if (lnk->vfs == class && lnk->ino == ino && lnk->dev == dev)
            return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check and made hardlink
 *
 * @return FALSE if the inode wasn't found in the cache and TRUE if it was found
 * and a hardlink was succesfully made
 */

static gboolean
check_hardlinks (const vfs_path_t * src_vpath, const vfs_path_t * dst_vpath, struct stat *pstat)
{
    GSList *lp;
    struct link *lnk;

    const struct vfs_class *my_vfs;
    ino_t ino = pstat->st_ino;
    dev_t dev = pstat->st_dev;
    struct stat link_stat;

    if ((vfs_file_class_flags (src_vpath) & VFSF_NOLINKS) != 0)
        return FALSE;

    my_vfs = vfs_path_get_by_index (src_vpath, -1)->class;

    for (lp = linklist; lp != NULL; lp = g_slist_next (lp))
    {
        lnk = (struct link *) lp->data;

        if (lnk->vfs == my_vfs && lnk->ino == ino && lnk->dev == dev)
        {
            const struct vfs_class *lp_name_class;
            int stat_result;

            lp_name_class = vfs_path_get_last_path_vfs (lnk->src_vpath);
            stat_result = mc_stat (lnk->src_vpath, &link_stat);

            if (stat_result == 0 && link_stat.st_ino == ino
                && link_stat.st_dev == dev && lp_name_class == my_vfs)
            {
                const struct vfs_class *p_class, *dst_name_class;

                dst_name_class = vfs_path_get_last_path_vfs (dst_vpath);
                p_class = vfs_path_get_last_path_vfs (lnk->dst_vpath);

                if (dst_name_class == p_class &&
                    mc_stat (lnk->dst_vpath, &link_stat) == 0 &&
                    mc_link (lnk->dst_vpath, dst_vpath) == 0)
                    return TRUE;
            }

            message (D_ERROR, MSG_ERROR, _("Cannot make the hardlink"));
            return FALSE;
        }
    }

    lnk = g_new0 (struct link, 1);
    if (lnk != NULL)
    {
        lnk->vfs = my_vfs;
        lnk->ino = ino;
        lnk->dev = dev;
        lnk->src_vpath = vfs_path_clone (src_vpath);
        lnk->dst_vpath = vfs_path_clone (dst_vpath);
        linklist = g_slist_prepend (linklist, lnk);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Duplicate the contents of the symbolic link src_path in dst_path.
 * Try to make a stable symlink if the option "stable symlink" was
 * set in the file mask dialog.
 * If dst_path is an existing symlink it will be deleted silently
 * (upper levels take already care of existing files at dst_path).
 */

static FileProgressStatus
make_symlink (FileOpContext * ctx, const char *src_path, const char *dst_path)
{
    char link_target[MC_MAXPATHLEN];
    int len;
    FileProgressStatus return_status;
    struct stat sb;
    vfs_path_t *src_vpath;
    vfs_path_t *dst_vpath;
    gboolean dst_is_symlink;
    vfs_path_t *link_target_vpath = NULL;

    src_vpath = vfs_path_from_str (src_path);
    dst_vpath = vfs_path_from_str (dst_path);
    dst_is_symlink = (mc_lstat (dst_vpath, &sb) == 0) && S_ISLNK (sb.st_mode);

  retry_src_readlink:
    len = mc_readlink (src_vpath, link_target, MC_MAXPATHLEN - 1);
    if (len < 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot read source link \"%s\"\n%s"), src_path);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_src_readlink;
        }
        goto ret;
    }
    link_target[len] = 0;

    if (ctx->stable_symlinks)
    {

        if (!vfs_file_is_local (src_vpath) || !vfs_file_is_local (dst_vpath))
        {
            message (D_ERROR, MSG_ERROR,
                     _("Cannot make stable symlinks across"
                       "non-local filesystems:\n\nOption Stable Symlinks will be disabled"));
            ctx->stable_symlinks = FALSE;
        }
    }

    if (ctx->stable_symlinks && !g_path_is_absolute (link_target))
    {
        char *p, *s;
        vfs_path_t *q;

        const char *r = strrchr (src_path, PATH_SEP);

        if (r)
        {
            p = g_strndup (src_path, r - src_path + 1);
            if (g_path_is_absolute (dst_path))
                q = vfs_path_from_str_flags (dst_path, VPF_NO_CANON);
            else
                q = vfs_path_build_filename (p, dst_path, (char *) NULL);

            if (vfs_path_tokens_count (q) > 1)
            {
                vfs_path_t *tmp_vpath1, *tmp_vpath2;

                tmp_vpath1 = vfs_path_vtokens_get (q, -1, 1);
                s = g_strconcat (p, link_target, (char *) NULL);
                g_free (p);
                g_strlcpy (link_target, s, sizeof (link_target));
                g_free (s);
                tmp_vpath2 = vfs_path_from_str (link_target);
                s = diff_two_paths (tmp_vpath1, tmp_vpath2);
                vfs_path_free (tmp_vpath1);
                vfs_path_free (tmp_vpath2);
                if (s)
                {
                    g_strlcpy (link_target, s, sizeof (link_target));
                    g_free (s);
                }
            }
            else
                g_free (p);
            vfs_path_free (q);
        }
    }
    link_target_vpath = vfs_path_from_str_flags (link_target, VPF_NO_CANON);

  retry_dst_symlink:
    if (mc_symlink (link_target_vpath, dst_vpath) == 0)
    {
        /* Success */
        return_status = FILE_CONT;
        goto ret;
    }
    /*
     * if dst_exists, it is obvious that this had failed.
     * We can delete the old symlink and try again...
     */
    if (dst_is_symlink)
    {
        if (mc_unlink (dst_vpath) == 0)
            if (mc_symlink (link_target_vpath, dst_vpath) == 0)
            {
                /* Success */
                return_status = FILE_CONT;
                goto ret;
            }
    }
    if (ctx->skip_all)
        return_status = FILE_SKIPALL;
    else
    {
        return_status = file_error (_("Cannot create target symlink \"%s\"\n%s"), dst_path);
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        if (return_status == FILE_RETRY)
            goto retry_dst_symlink;
    }

  ret:
    vfs_path_free (src_vpath);
    vfs_path_free (dst_vpath);
    vfs_path_free (link_target_vpath);
    return return_status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * do_compute_dir_size:
 *
 * Computes the number of bytes used by the files in a directory
 */

static FileProgressStatus
do_compute_dir_size (const vfs_path_t * dirname_vpath, void *ui,
                     compute_dir_size_callback cback, size_t * dir_count, size_t * ret_marked,
                     uintmax_t * ret_total, gboolean compute_symlinks)
{
    static unsigned short int update_ui_count = 0;

    int res;
    struct stat s;
    DIR *dir;
    struct dirent *dirent;
    FileProgressStatus ret = FILE_CONT;

    if (!compute_symlinks)
    {
        res = mc_lstat (dirname_vpath, &s);
        if (res != 0)
            return ret;

        /* don't scan symlink to directory */
        if (S_ISLNK (s.st_mode))
        {
            (*ret_marked)++;
            *ret_total += (uintmax_t) s.st_size;
            return ret;
        }
    }

    (*dir_count)++;

    dir = mc_opendir (dirname_vpath);
    if (dir == NULL)
        return ret;

    while (ret == FILE_CONT && (dirent = mc_readdir (dir)) != NULL)
    {
        vfs_path_t *tmp_vpath;

        if (strcmp (dirent->d_name, ".") == 0)
            continue;
        if (strcmp (dirent->d_name, "..") == 0)
            continue;

        tmp_vpath = vfs_path_append_new (dirname_vpath, dirent->d_name, NULL);

        res = mc_lstat (tmp_vpath, &s);
        if (res == 0)
        {
            if (S_ISDIR (s.st_mode))
            {
                ret =
                    do_compute_dir_size (tmp_vpath, ui, cback, dir_count, ret_marked, ret_total,
                                         compute_symlinks);
                if (ret == FILE_CONT)
                    ret =
                        (cback == NULL) ? FILE_CONT : cback (ui, tmp_vpath, *dir_count, *ret_total);
            }
            else
            {
                (*ret_marked)++;
                *ret_total += (uintmax_t) s.st_size;

                update_ui_count++;
                if ((update_ui_count & 31) == 0)
                    ret =
                        (cback == NULL) ? FILE_CONT : cback (ui, tmp_vpath, *dir_count, *ret_total);
            }
        }

        vfs_path_free (tmp_vpath);
    }

    mc_closedir (dir);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
progress_update_one (FileOpTotalContext * tctx, FileOpContext * ctx, off_t add)
{
    struct timeval tv_current;
    static struct timeval tv_start = { };

    tctx->progress_count++;
    tctx->progress_bytes += (uintmax_t) add;

    if (tv_start.tv_sec == 0)
    {
        gettimeofday (&tv_start, (struct timezone *) NULL);
    }
    gettimeofday (&tv_current, (struct timezone *) NULL);
    if ((tv_current.tv_sec - tv_start.tv_sec) > FILEOP_UPDATE_INTERVAL)
    {
        if (verbose && ctx->dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
        {
            file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
            file_progress_show_total (tctx, ctx, tctx->progress_bytes, TRUE);
        }
        tv_start.tv_sec = tv_current.tv_sec;
    }

    return check_progress_buttons (ctx);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
real_warn_same_file (enum OperationMode mode, const char *fmt, const char *a, const char *b)
{
    char *msg;
    int result = 0;
    const char *head_msg;

    head_msg = mode == Foreground ? MSG_ERROR : _("Background process error");

    msg = g_strdup_printf (fmt, a, b);
    result = query_dialog (head_msg, msg, D_ERROR, 2, _("&Skip"), _("&Abort"));
    g_free (msg);
    do_refresh ();

    return (result == 1) ? FILE_ABORT : FILE_SKIP;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
warn_same_file (const char *fmt, const char *a, const char *b)
{
#ifdef ENABLE_BACKGROUND
/* *INDENT-OFF* */
    union
    {
        void *p;
          FileProgressStatus (*f) (enum OperationMode, const char *fmt,
                                   const char *a, const char *b);
    } pntr;
/* *INDENT-ON* */

    pntr.f = real_warn_same_file;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, NULL, 3, strlen (fmt), fmt, strlen (a), a, strlen (b), b);
#endif
    return real_warn_same_file (Foreground, fmt, a, b);
}

/* --------------------------------------------------------------------------------------------- */
/* {{{ Query/status report routines */

static FileProgressStatus
real_do_file_error (enum OperationMode mode, const char *error)
{
    int result;
    const char *msg;

    msg = mode == Foreground ? MSG_ERROR : _("Background process error");
    result =
        query_dialog (msg, error, D_ERROR, 4, _("&Skip"), _("Ski&p all"), _("&Retry"), _("&Abort"));

    switch (result)
    {
    case 0:
        do_refresh ();
        return FILE_SKIP;

    case 1:
        do_refresh ();
        return FILE_SKIPALL;

    case 2:
        do_refresh ();
        return FILE_RETRY;

    case 3:
    default:
        return FILE_ABORT;
    }
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
real_query_recursive (FileOpContext * ctx, enum OperationMode mode, const char *s)
{

    if (ctx->recursive_result < RECURSIVE_ALWAYS)
    {

        const char *msg;
        char *text;

        msg = mode == Foreground
            ? _("Directory \"%s\" not empty.\nDelete it recursively?")
            : _("Background process:\nDirectory \"%s\" not empty.\nDelete it recursively?");
        text = g_strdup_printf (msg, path_trunc (s, 30));

        if (safe_delete)
            query_set_sel (1);

        ctx->recursive_result =
            (FileCopyMode) query_dialog (op_names[OP_DELETE], text, D_ERROR, 5,
                                         _("&Yes"), _("&No"), _("A&ll"), _("Non&e"), _("&Abort"));
        g_free (text);

        if (ctx->recursive_result != RECURSIVE_ABORT)
            do_refresh ();
    }

    switch (ctx->recursive_result)
    {
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

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static FileProgressStatus
do_file_error (const char *str)
{
    union
    {
        void *p;
          FileProgressStatus (*f) (enum OperationMode, const char *);
    } pntr;
    pntr.f = real_do_file_error;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, NULL, 1, strlen (str), str);
    else
        return real_do_file_error (Foreground, str);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_recursive (FileOpContext * ctx, const char *s)
{
    union
    {
        void *p;
          FileProgressStatus (*f) (FileOpContext *, enum OperationMode, const char *);
    } pntr;
    pntr.f = real_query_recursive;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, ctx, 1, strlen (s), s);
    else
        return real_query_recursive (ctx, Foreground, s);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_replace (FileOpContext * ctx, const char *destname, struct stat *_s_stat,
               struct stat *_d_stat)
{
    union
    {
        void *p;
          FileProgressStatus (*f) (FileOpContext *, enum OperationMode, const char *,
                                   struct stat *, struct stat *);
    } pntr;
    pntr.f = file_progress_real_query_replace;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, ctx, 3, strlen (destname), destname,
                            sizeof (struct stat), _s_stat, sizeof (struct stat), _d_stat);
    else
        return file_progress_real_query_replace (ctx, Foreground, destname, _s_stat, _d_stat);
}

#else
/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
do_file_error (const char *str)
{
    return real_do_file_error (Foreground, str);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_recursive (FileOpContext * ctx, const char *s)
{
    return real_query_recursive (ctx, Foreground, s);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_replace (FileOpContext * ctx, const char *destname, struct stat *_s_stat,
               struct stat *_d_stat)
{
    return file_progress_real_query_replace (ctx, Foreground, destname, _s_stat, _d_stat);
}

#endif /* !ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/** Report error with two files */

static FileProgressStatus
files_error (const char *format, const char *file1, const char *file2)
{
    char buf[BUF_MEDIUM];
    char *nfile1 = g_strdup (path_trunc (file1, 15));
    char *nfile2 = g_strdup (path_trunc (file2, 15));

    g_snprintf (buf, sizeof (buf), format, nfile1, nfile2, unix_error_string (errno));

    g_free (nfile1);
    g_free (nfile2);

    return do_file_error (buf);
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */

static void
copy_file_file_display_progress (FileOpTotalContext * tctx, FileOpContext * ctx,
                                 struct timeval tv_current, struct timeval tv_transfer_start,
                                 off_t file_size, off_t n_read_total)
{
    long dt;

    /* 1. Update rotating dash after some time */
    rotate_dash ();

    /* 3. Compute ETA */
    dt = (tv_current.tv_sec - tv_transfer_start.tv_sec);

    if (n_read_total == 0)
        ctx->eta_secs = 0.0;
    else
    {
        ctx->eta_secs = ((dt / (double) n_read_total) * file_size) - dt;
        ctx->bps = n_read_total / ((dt < 1) ? 1 : dt);
    }

    /* 4. Compute BPS rate */
    ctx->bps_time = (tv_current.tv_sec - tv_transfer_start.tv_sec);
    if (ctx->bps_time < 1)
        ctx->bps_time = 1;
    ctx->bps = n_read_total / ctx->bps_time;

    /* 5. Compute total ETA and BPS */
    if (ctx->progress_bytes != 0)
    {
        uintmax_t remain_bytes;

        remain_bytes = ctx->progress_bytes - tctx->copied_bytes;
#if 1
        {
            int total_secs = tv_current.tv_sec - tctx->transfer_start.tv_sec;

            if (total_secs < 1)
                total_secs = 1;

            tctx->bps = tctx->copied_bytes / total_secs;
            tctx->eta_secs = (tctx->bps != 0) ? remain_bytes / tctx->bps : 0;
        }
#else
        /* broken on lot of little files */
        tctx->bps_count++;
        tctx->bps = (tctx->bps * (tctx->bps_count - 1) + ctx->bps) / tctx->bps_count;
        tctx->eta_secs = (tctx->bps != 0) ? remain_bytes / tctx->bps : 0;
#endif
    }
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Move routines */
static FileProgressStatus
move_file_file (FileOpTotalContext * tctx, FileOpContext * ctx, const char *s, const char *d)
{
    struct stat src_stats, dst_stats;
    FileProgressStatus return_status = FILE_CONT;
    gboolean copy_done = FALSE;
    gboolean old_ask_overwrite;
    vfs_path_t *src_vpath, *dst_vpath;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    file_progress_show_source (ctx, src_vpath);
    file_progress_show_target (ctx, dst_vpath);

    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret;
    }

    mc_refresh ();

    while (mc_lstat (src_vpath, &src_stats) != 0)
    {
        /* Source doesn't exist */
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot stat file \"%s\"\n%s"), s);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }

        if (return_status != FILE_RETRY)
            goto ret;
    }

    if (mc_lstat (dst_vpath, &dst_stats) == 0)
    {
        if (src_stats.st_dev == dst_stats.st_dev && src_stats.st_ino == dst_stats.st_ino)
        {
            return_status = warn_same_file (_("\"%s\"\nand\n\"%s\"\nare the same file"), s, d);
            goto ret;
        }

        if (S_ISDIR (dst_stats.st_mode))
        {
            message (D_ERROR, MSG_ERROR, _("Cannot overwrite directory \"%s\""), d);
            do_refresh ();
            return_status = FILE_SKIP;
            goto ret;
        }

        if (confirm_overwrite)
        {
            return_status = query_replace (ctx, d, &src_stats, &dst_stats);
            if (return_status != FILE_CONT)
                goto ret;
        }
        /* Ok to overwrite */
    }

    if (!ctx->do_append)
    {
        if (S_ISLNK (src_stats.st_mode) && ctx->stable_symlinks)
        {
            return_status = make_symlink (ctx, s, d);
            if (return_status == FILE_CONT)
                goto retry_src_remove;
            goto ret;
        }

        if (mc_rename (src_vpath, dst_vpath) == 0)
        {
            return_status = progress_update_one (tctx, ctx, src_stats.st_size);
            goto ret;
        }
    }
#if 0
    /* Comparison to EXDEV seems not to work in nfs if you're moving from
       one nfs to the same, but on the server it is on two different
       filesystems. Then nfs returns EIO instead of EXDEV.
       Hope it will not hurt if we always in case of error try to copy/delete. */
    else
        errno = EXDEV;          /* Hack to copy (append) the file and then delete it */

    if (errno != EXDEV)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = files_error (_("Cannot move file \"%s\" to \"%s\"\n%s"), s, d);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_rename;
        }

        goto ret;
    }
#endif

    /* Failed because filesystem boundary -> copy the file instead */
    old_ask_overwrite = tctx->ask_overwrite;
    tctx->ask_overwrite = FALSE;
    return_status = copy_file_file (tctx, ctx, s, d);
    tctx->ask_overwrite = old_ask_overwrite;
    if (return_status != FILE_CONT)
        goto ret;

    copy_done = TRUE;

    file_progress_show_source (ctx, NULL);
    file_progress_show (ctx, 0, 0, "", FALSE);

    return_status = check_progress_buttons (ctx);
    if (return_status != FILE_CONT)
        goto ret;
    mc_refresh ();

  retry_src_remove:
    if (mc_unlink (src_vpath) != 0 && !ctx->skip_all)
    {
        return_status = file_error (_("Cannot remove file \"%s\"\n%s"), s);
        if (return_status == FILE_RETRY)
            goto retry_src_remove;
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        goto ret;
    }

    if (!copy_done)
        return_status = progress_update_one (tctx, ctx, src_stats.st_size);

  ret:
    vfs_path_free (src_vpath);
    vfs_path_free (dst_vpath);

    return return_status;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Erase routines */
/** Don't update progress status if progress_count==NULL */

static FileProgressStatus
erase_file (FileOpTotalContext * tctx, FileOpContext * ctx, const vfs_path_t * vpath)
{
    int return_status;
    struct stat buf;
    char *s;

    s = vfs_path_to_str (vpath);
    file_progress_show_deleting (ctx, s);
    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        g_free (s);
        return FILE_ABORT;
    }
    mc_refresh ();

    if (tctx->progress_count != 0 && mc_lstat (vpath, &buf) != 0)
    {
        /* ignore, most likely the mc_unlink fails, too */
        buf.st_size = 0;
    }

    while (mc_unlink (vpath) != 0 && !ctx->skip_all)
    {
        return_status = file_error (_("Cannot delete file \"%s\"\n%s"), s);
        if (return_status == FILE_ABORT)
        {
            g_free (s);
            return return_status;
        }
        if (return_status == FILE_RETRY)
            continue;
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        break;
    }
    g_free (s);
    if (tctx->progress_count == 0)
        return FILE_CONT;
    return progress_update_one (tctx, ctx, buf.st_size);
}

/* --------------------------------------------------------------------------------------------- */

/**
  Recursive remove of files
  abort->cancel stack
  skip ->warn every level, gets default
  skipall->remove as much as possible
*/
static FileProgressStatus
recursive_erase (FileOpTotalContext * tctx, FileOpContext * ctx, const char *s)
{
    struct dirent *next;
    struct stat buf;
    DIR *reading;
    char *path;
    FileProgressStatus return_status = FILE_CONT;
    vfs_path_t *vpath;

    if (strcmp (s, "..") == 0)
        return FILE_RETRY;

    vpath = vfs_path_from_str (s);
    reading = mc_opendir (vpath);

    if (reading == NULL)
    {
        return_status = FILE_RETRY;
        goto ret;
    }

    while ((next = mc_readdir (reading)) && return_status != FILE_ABORT)
    {
        vfs_path_t *tmp_vpath;

        if (!strcmp (next->d_name, "."))
            continue;
        if (!strcmp (next->d_name, ".."))
            continue;
        path = mc_build_filename (s, next->d_name, NULL);
        tmp_vpath = vfs_path_from_str (path);
        if (mc_lstat (tmp_vpath, &buf) != 0)
        {
            g_free (path);
            mc_closedir (reading);
            vfs_path_free (tmp_vpath);
            return_status = FILE_RETRY;
            goto ret;
        }
        if (S_ISDIR (buf.st_mode))
            return_status = recursive_erase (tctx, ctx, path);
        else
            return_status = erase_file (tctx, ctx, tmp_vpath);
        vfs_path_free (tmp_vpath);
        g_free (path);
    }
    mc_closedir (reading);
    if (return_status == FILE_ABORT)
        goto ret;

    file_progress_show_deleting (ctx, s);
    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret;
    }
    mc_refresh ();

    while (my_rmdir (s) != 0 && !ctx->skip_all)
    {
        return_status = file_error (_("Cannot remove directory \"%s\"\n%s"), s);
        if (return_status == FILE_RETRY)
            continue;
        if (return_status == FILE_ABORT)
            goto ret;
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        break;
    }

  ret:
    vfs_path_free (vpath);
    return return_status;
}

/* --------------------------------------------------------------------------------------------- */
/** Return -1 on error, 1 if there are no entries besides "." and ".." 
   in the directory path points to, 0 else. */

static int
check_dir_is_empty (const vfs_path_t * vpath)
{
    DIR *dir;
    struct dirent *d;
    int i;

    dir = mc_opendir (vpath);
    if (!dir)
        return -1;

    for (i = 1, d = mc_readdir (dir); d; d = mc_readdir (dir))
    {
        if (d->d_name[0] == '.' && (d->d_name[1] == '\0' ||
                                    (d->d_name[1] == '.' && d->d_name[2] == '\0')))
            continue;           /* "." or ".." */
        i = 0;
        break;
    }

    mc_closedir (dir);
    return i;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
erase_dir_iff_empty (FileOpContext * ctx, const char *s)
{
    FileProgressStatus error;
    vfs_path_t *s_vpath;

    if (strcmp (s, "..") == 0)
        return FILE_SKIP;

    if (strcmp (s, ".") == 0)
        return FILE_SKIP;

    file_progress_show_deleting (ctx, s);
    if (check_progress_buttons (ctx) == FILE_ABORT)
        return FILE_ABORT;

    mc_refresh ();

    s_vpath = vfs_path_from_str (s);

    if (check_dir_is_empty (s_vpath) == 1)      /* not empty or error */
    {
        while (my_rmdir (s) != 0 && !ctx->skip_all)
        {
            error = file_error (_("Cannot remove directory \"%s\"\n%s"), s);
            if (error == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (error != FILE_RETRY)
            {
                vfs_path_free (s_vpath);
                return error;
            }
        }
    }

    vfs_path_free (s_vpath);
    return FILE_CONT;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Panel operate routines */

/**
 * Return currently selected entry name or the name of the first marked
 * entry if there is one.
 */

static char *
panel_get_file (WPanel * panel)
{
    if (get_current_type () == view_tree)
    {
        WTree *tree;

        tree = (WTree *) get_panel_widget (get_current_index ());
        return vfs_path_to_str (tree_selected_name (tree));
    }

    if (panel->marked != 0)
    {
        int i;

        for (i = 0; i < panel->count; i++)
            if (panel->dir.list[i].f.marked)
                return g_strdup (panel->dir.list[i].fname);
    }
    return g_strdup (panel->dir.list[panel->selected].fname);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * panel_compute_totals:
 *
 * compute the number of files and the number of bytes
 * used up by the whole selection, recursing directories
 * as required.  In addition, it checks to see if it will
 * overwrite any files by doing the copy.
 */

static FileProgressStatus
panel_compute_totals (const WPanel * panel, void *ui, compute_dir_size_callback cback,
                      size_t * ret_marked, uintmax_t * ret_total, gboolean compute_symlinks)
{
    int i;

    for (i = 0; i < panel->count; i++)
    {
        struct stat *s;

        if (!panel->dir.list[i].f.marked)
            continue;

        s = &panel->dir.list[i].st;

        if (S_ISDIR (s->st_mode))
        {
            vfs_path_t *p;
            FileProgressStatus status;

            p = vfs_path_append_new (panel->cwd_vpath, panel->dir.list[i].fname, NULL);
            status = compute_dir_size (p, ui, cback, ret_marked, ret_total, compute_symlinks);
            vfs_path_free (p);

            if (status != FILE_CONT)
                return status;
        }
        else
        {
            (*ret_marked)++;
            *ret_total += (uintmax_t) s->st_size;
        }
    }

    return FILE_CONT;
}

/* --------------------------------------------------------------------------------------------- */

/** Initialize variables for progress bars */
static FileProgressStatus
panel_operate_init_totals (FileOperation operation, const WPanel * panel, const char *source,
                           FileOpContext * ctx, filegui_dialog_type_t dialog_type)
{
    FileProgressStatus status;

#ifdef ENABLE_BACKGROUND
    if (mc_global.we_are_background)
        return FILE_CONT;
#endif

    if (operation != OP_MOVE && verbose && file_op_compute_totals)
    {
        ComputeDirSizeUI *ui;

        ui = compute_dir_size_create_ui (TRUE);

        ctx->progress_count = 0;
        ctx->progress_bytes = 0;

        if (source == NULL)
            status = panel_compute_totals (panel, ui, compute_dir_size_update_ui,
                                           &ctx->progress_count, &ctx->progress_bytes,
                                           ctx->follow_links);
        else
        {
            vfs_path_t *p;

            p = vfs_path_from_str (source);
            status = compute_dir_size (p, ui, compute_dir_size_update_ui,
                                       &ctx->progress_count, &ctx->progress_bytes,
                                       ctx->follow_links);
            vfs_path_free (p);
        }

        compute_dir_size_destroy_ui (ui);

        ctx->progress_totals_computed = (status == FILE_CONT);

        if (status == FILE_SKIP)
            status = FILE_CONT;
    }
    else
    {
        status = FILE_CONT;
        ctx->progress_count = panel->marked;
        ctx->progress_bytes = panel->total;
        ctx->progress_totals_computed = FALSE;
    }

    file_op_context_create_ui (ctx, TRUE, dialog_type);

    return status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Generate user prompt for panel operation.
 * single_source is the name if the source entry or NULL for multiple
 * entries.
 * src_stat is only used when single_source is not NULL.
 */

static char *
panel_operate_generate_prompt (const WPanel * panel, FileOperation operation,
                               gboolean single_source, const struct stat *src_stat)
{
    const char *sp, *cp;
    char format_string[BUF_MEDIUM];
    char *dp = format_string;
    gboolean build_question = FALSE;

    static gboolean i18n_flag = FALSE;
    if (!i18n_flag)
    {
        size_t i;

        for (i = sizeof (op_names1) / sizeof (op_names1[0]); i--;)
            op_names1[i] = Q_ (op_names1[i]);

#ifdef ENABLE_NLS
        for (i = sizeof (prompt_parts) / sizeof (prompt_parts[0]); i--;)
            prompt_parts[i] = _(prompt_parts[i]);

        one_format = _(one_format);
        many_format = _(many_format);
        question_format = _(question_format);
#endif /* ENABLE_NLS */
        i18n_flag = TRUE;
    }

    sp = single_source ? one_format : many_format;

    while (*sp != '\0')
    {
        switch (*sp)
        {
        case '%':
            cp = NULL;
            switch (sp[1])
            {
            case 'o':
                cp = op_names1[operation];
                break;
            case 'm':
                if (operation == OP_DELETE)
                {
                    cp = "";
                    build_question = TRUE;
                }
                else
                    cp = prompt_parts[5];
                break;
            case 'e':
                if (operation == OP_DELETE)
                {
                    cp = "";
                    build_question = TRUE;
                }
                else
                    cp = prompt_parts[6];
                break;
            case 'f':
                if (single_source)
                    cp = S_ISDIR (src_stat->st_mode) ? prompt_parts[2] : prompt_parts[0];
                else
                    cp = (panel->marked == panel->dirs_marked)
                        ? prompt_parts[3]
                        : (panel->dirs_marked ? prompt_parts[4] : prompt_parts[1]);
                break;
            default:
                *dp++ = *sp++;
            }

            if (cp != NULL)
            {
                sp += 2;

                while (*cp != '\0')
                    *dp++ = *cp++;

                /* form two-lines query prompt for file deletion */
                if (operation == OP_DELETE && sp[-1] == 'f')
                {
                    *dp++ = '\n';

                    while (isblank (*sp) != 0)
                        sp++;
                }
            }
            break;
        default:
            *dp++ = *sp++;
        }
    }
    *dp = '\0';

    if (build_question)
    {
        char tmp[BUF_MEDIUM];

        memmove (tmp, format_string, sizeof (tmp));
        g_snprintf (format_string, sizeof (format_string), question_format, tmp);
    }

    return g_strdup (format_string);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static int
end_bg_process (FileOpContext * ctx, enum OperationMode mode)
{
    int pid = ctx->pid;

    (void) mode;
    ctx->pid = 0;

    unregister_task_with_pid (pid);
    /*     file_op_context_destroy(ctx); */
    return 1;
}
#endif
/* }}} */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
copy_file_file (FileOpTotalContext * tctx, FileOpContext * ctx,
                const char *src_path, const char *dst_path)
{
    uid_t src_uid = (uid_t) (-1);
    gid_t src_gid = (gid_t) (-1);

    int src_desc, dest_desc = -1;
    int n_read, n_written;
    mode_t src_mode = 0;        /* The mode of the source file */
    struct stat sb, sb2;
    struct utimbuf utb;
    gboolean dst_exists = FALSE, appending = FALSE;
    off_t file_size = -1;
    FileProgressStatus return_status, temp_status;
    struct timeval tv_transfer_start;
    dest_status_t dst_status = DEST_NONE;
    int open_flags;
    gboolean is_first_time = TRUE;
    vfs_path_t *src_vpath = NULL, *dst_vpath = NULL;
    gboolean write_errno_nospace = FALSE;

    /* FIXME: We should not be using global variables! */
    ctx->do_reget = 0;
    return_status = FILE_RETRY;

    dst_vpath = vfs_path_from_str (dst_path);
    src_vpath = vfs_path_from_str (src_path);

    file_progress_show_source (ctx, src_vpath);
    file_progress_show_target (ctx, dst_vpath);

    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret_fast;
    }

    mc_refresh ();

    while (mc_stat (dst_vpath, &sb2) == 0)
    {
        if (S_ISDIR (sb2.st_mode))
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status = file_error (_("Cannot overwrite directory \"%s\"\n%s"), dst_path);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                if (return_status == FILE_RETRY)
                    continue;
            }
            goto ret_fast;
        }

        dst_exists = TRUE;
        break;
    }

    while ((*ctx->stat_func) (src_vpath, &sb) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot stat source file \"%s\"\n%s"), src_path);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }

        if (return_status != FILE_RETRY)
            goto ret_fast;
    }

    if (dst_exists)
    {
        /* Destination already exists */
        if (sb.st_dev == sb2.st_dev && sb.st_ino == sb2.st_ino)
        {
            return_status = warn_same_file (_("\"%s\"\nand\n\"%s\"\nare the same file"),
                                            src_path, dst_path);
            goto ret_fast;
        }

        /* Should we replace destination? */
        if (tctx->ask_overwrite)
        {
            ctx->do_reget = 0;
            return_status = query_replace (ctx, dst_path, &sb, &sb2);
            if (return_status != FILE_CONT)
                goto ret_fast;
        }
    }

    if (!ctx->do_append)
    {
        /* Check the hardlinks */
        if (!ctx->follow_links && sb.st_nlink > 1 && check_hardlinks (src_vpath, dst_vpath, &sb))
        {
            /* We have made a hardlink - no more processing is necessary */
            return_status = FILE_CONT;
            goto ret_fast;
        }

        if (S_ISLNK (sb.st_mode))
        {
            return_status = make_symlink (ctx, src_path, dst_path);
            goto ret_fast;
        }

        if (S_ISCHR (sb.st_mode) || S_ISBLK (sb.st_mode) ||
            S_ISFIFO (sb.st_mode) || S_ISNAM (sb.st_mode) || S_ISSOCK (sb.st_mode))
        {
            while (mc_mknod (dst_vpath, sb.st_mode & ctx->umask_kill, sb.st_rdev) < 0
                   && !ctx->skip_all)
            {
                return_status = file_error (_("Cannot create special file \"%s\"\n%s"), dst_path);
                if (return_status == FILE_RETRY)
                    continue;
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                goto ret_fast;
            }
            /* Success */

            while (ctx->preserve_uidgid && mc_chown (dst_vpath, sb.st_uid, sb.st_gid) != 0
                   && !ctx->skip_all)
            {
                temp_status = file_error (_("Cannot chown target file \"%s\"\n%s"), dst_path);
                if (temp_status == FILE_SKIP)
                    break;
                if (temp_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                if (temp_status != FILE_RETRY)
                {
                    return_status = temp_status;
                    goto ret_fast;
                }
            }

            while (ctx->preserve && mc_chmod (dst_vpath, sb.st_mode & ctx->umask_kill) != 0
                   && !ctx->skip_all)
            {
                temp_status = file_error (_("Cannot chmod target file \"%s\"\n%s"), dst_path);
                if (temp_status == FILE_SKIP)
                    break;
                if (temp_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                if (temp_status != FILE_RETRY)
                {
                    return_status = temp_status;
                    goto ret_fast;
                }
            }

            return_status = FILE_CONT;
            goto ret_fast;
        }
    }

    gettimeofday (&tv_transfer_start, (struct timezone *) NULL);

    while ((src_desc = mc_open (src_vpath, O_RDONLY | O_LINEAR)) < 0 && !ctx->skip_all)
    {
        return_status = file_error (_("Cannot open source file \"%s\"\n%s"), src_path);
        if (return_status == FILE_RETRY)
            continue;
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        if (return_status == FILE_SKIP)
            break;
        ctx->do_append = 0;
        goto ret_fast;
    }

    if (ctx->do_reget != 0)
    {
        if (mc_lseek (src_desc, ctx->do_reget, SEEK_SET) != ctx->do_reget)
        {
            message (D_ERROR, _("Warning"), _("Reget failed, about to overwrite file"));
            ctx->do_reget = 0;
            ctx->do_append = FALSE;
        }
    }

    while (mc_fstat (src_desc, &sb) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot fstat source file \"%s\"\n%s"), src_path);
            if (return_status == FILE_RETRY)
                continue;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            ctx->do_append = FALSE;
        }
        goto ret;
    }

    src_mode = sb.st_mode;
    src_uid = sb.st_uid;
    src_gid = sb.st_gid;
    utb.actime = sb.st_atime;
    utb.modtime = sb.st_mtime;
    file_size = sb.st_size;

    open_flags = O_WRONLY;
    if (dst_exists)
    {
        if (ctx->do_append != 0)
            open_flags |= O_APPEND;
        else
            open_flags |= O_CREAT | O_TRUNC;
    }
    else
    {
        open_flags |= O_CREAT | O_EXCL;
    }

    while ((dest_desc = mc_open (dst_vpath, open_flags, src_mode)) < 0)
    {
        if (errno != EEXIST)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status = file_error (_("Cannot create target file \"%s\"\n%s"), dst_path);
                if (return_status == FILE_RETRY)
                    continue;
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                ctx->do_append = FALSE;
            }
        }
        goto ret;
    }
    dst_status = DEST_SHORT;    /* file opened, but not fully copied */

    appending = ctx->do_append;
    ctx->do_append = FALSE;

    /* Find out the optimal buffer size.  */
    while (mc_fstat (dest_desc, &sb) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot fstat target file \"%s\"\n%s"), dst_path);
            if (return_status == FILE_RETRY)
                continue;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret;
    }

    while (TRUE)
    {
        errno = vfs_preallocate (dest_desc, file_size, (ctx->do_append != 0) ? sb.st_size : 0);
        if (errno == 0)
            break;

        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status =
                file_error (_("Cannot preallocate space for target file \"%s\"\n%s"), dst_path);
            if (return_status == FILE_RETRY)
                continue;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        mc_close (dest_desc);
        dest_desc = -1;
        mc_unlink (dst_vpath);
        dst_status = DEST_NONE;
        goto ret;
    }

    ctx->eta_secs = 0.0;
    ctx->bps = 0;

    if (tctx->bps == 0 || (file_size / (tctx->bps)) > FILEOP_UPDATE_INTERVAL)
        file_progress_show (ctx, 0, file_size, "", TRUE);
    else
        file_progress_show (ctx, 1, 1, "", TRUE);
    return_status = check_progress_buttons (ctx);
    mc_refresh ();

    if (return_status != FILE_CONT)
        goto ret;

    {
        off_t n_read_total = 0;
        struct timeval tv_current, tv_last_update, tv_last_input;
        int secs, update_secs;
        const char *stalled_msg = "";

        tv_last_update = tv_transfer_start;

        while (TRUE)
        {
            char buf[BUF_8K];

            /* src_read */
            if (mc_ctl (src_desc, VFS_CTL_IS_NOTREADY, 0))
                n_read = -1;
            else
                while ((n_read = mc_read (src_desc, buf, sizeof (buf))) < 0 && !ctx->skip_all)
                {
                    return_status = file_error (_("Cannot read source file\"%s\"\n%s"), src_path);
                    if (return_status == FILE_RETRY)
                        continue;
                    if (return_status == FILE_SKIPALL)
                        ctx->skip_all = TRUE;
                    goto ret;
                }
            if (n_read == 0)
                break;

            gettimeofday (&tv_current, NULL);

            if (n_read > 0)
            {
                char *t = buf;
                n_read_total += n_read;

                /* Windows NT ftp servers report that files have no
                 * permissions: -------, so if we happen to have actually
                 * read something, we should fix the permissions.
                 */
                if ((src_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == 0)
                    src_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                gettimeofday (&tv_last_input, NULL);

                /* dst_write */
                while ((n_written = mc_write (dest_desc, t, n_read)) < n_read)
                {
                    if (n_written > 0)
                    {
                        n_read -= n_written;
                        t += n_written;
                        continue;
                    }

                    write_errno_nospace = (n_written < 0 && errno == ENOSPC);

                    if (ctx->skip_all)
                        return_status = FILE_SKIPALL;
                    else
                        return_status =
                            file_error (_("Cannot write target file \"%s\"\n%s"), dst_path);

                    if (return_status == FILE_SKIP)
                    {
                        if (write_errno_nospace)
                            goto ret;
                        break;
                    }
                    if (return_status == FILE_SKIPALL)
                    {
                        ctx->skip_all = TRUE;
                        if (write_errno_nospace)
                            goto ret;
                    }
                    if (return_status != FILE_RETRY)
                        goto ret;

                    /* User pressed "Retry". Will the next mc_write() call be succesful?
                     * Reset error flag to be ready for that. */
                    write_errno_nospace = FALSE;
                }
            }

            tctx->copied_bytes = tctx->progress_bytes + n_read_total + ctx->do_reget;

            secs = (tv_current.tv_sec - tv_last_update.tv_sec);
            update_secs = (tv_current.tv_sec - tv_last_input.tv_sec);

            if (is_first_time || secs > FILEOP_UPDATE_INTERVAL)
            {
                copy_file_file_display_progress (tctx, ctx,
                                                 tv_current,
                                                 tv_transfer_start, file_size, n_read_total);
                tv_last_update = tv_current;
            }
            is_first_time = FALSE;

            if (update_secs > FILEOP_STALLING_INTERVAL)
            {
                stalled_msg = _("(stalled)");
            }

            {
                gboolean force_update;

                force_update =
                    (tv_current.tv_sec - tctx->transfer_start.tv_sec) > FILEOP_UPDATE_INTERVAL;

                if (verbose && ctx->dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
                {
                    file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
                    file_progress_show_total (tctx, ctx, tctx->copied_bytes, force_update);
                }

                file_progress_show (ctx, n_read_total + ctx->do_reget, file_size, stalled_msg,
                                    force_update);
            }
            mc_refresh ();

            return_status = check_progress_buttons (ctx);

            if (return_status != FILE_CONT)
            {
                mc_refresh ();
                goto ret;
            }
        }
    }

    dst_status = DEST_FULL;     /* copy successful, don't remove target file */

  ret:
    while (src_desc != -1 && mc_close (src_desc) < 0 && !ctx->skip_all)
    {
        temp_status = file_error (_("Cannot close source file \"%s\"\n%s"), src_path);
        if (temp_status == FILE_RETRY)
            continue;
        if (temp_status == FILE_ABORT)
            return_status = temp_status;
        if (temp_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        break;
    }

    while (dest_desc != -1 && mc_close (dest_desc) < 0 && !ctx->skip_all)
    {
        temp_status = file_error (_("Cannot close target file \"%s\"\n%s"), dst_path);
        if (temp_status == FILE_RETRY)
            continue;
        if (temp_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        return_status = temp_status;
        break;
    }

    if (dst_status == DEST_SHORT)
    {
        /* Remove short file */
        int result = 0;

        /* In case of copy/move to full partition, keep source file
         * and remove incomplete destination one */
        if (!write_errno_nospace)
            result = query_dialog (Q_ ("DialogTitle|Copy"),
                                   _("Incomplete file was retrieved. Keep it?"),
                                   D_ERROR, 2, _("&Delete"), _("&Keep"));
        if (result == 0)
            mc_unlink (dst_vpath);
    }
    else if (dst_status == DEST_FULL)
    {
        /* Copy has succeeded */
        if (!appending && ctx->preserve_uidgid)
        {
            while (mc_chown (dst_vpath, src_uid, src_gid) != 0 && !ctx->skip_all)
            {
                temp_status = file_error (_("Cannot chown target file \"%s\"\n%s"), dst_path);
                if (temp_status == FILE_RETRY)
                    continue;
                if (temp_status == FILE_SKIPALL)
                {
                    ctx->skip_all = TRUE;
                    return_status = FILE_CONT;
                }
                if (temp_status == FILE_SKIP)
                    return_status = FILE_CONT;
                break;
            }
        }

        if (!appending)
        {
            if (ctx->preserve)
            {
                while (mc_chmod (dst_vpath, (src_mode & ctx->umask_kill)) != 0 && !ctx->skip_all)
                {
                    temp_status = file_error (_("Cannot chmod target file \"%s\"\n%s"), dst_path);
                    if (temp_status == FILE_RETRY)
                        continue;
                    if (temp_status == FILE_SKIPALL)
                    {
                        ctx->skip_all = TRUE;
                        return_status = FILE_CONT;
                    }
                    if (temp_status == FILE_SKIP)
                        return_status = FILE_CONT;
                    break;
                }
            }
            else if (!dst_exists)
            {
                src_mode = umask (-1);
                umask (src_mode);
                src_mode = 0100666 & ~src_mode;
                mc_chmod (dst_vpath, (src_mode & ctx->umask_kill));
            }
            mc_utime (dst_vpath, &utb);
        }
    }

    if (return_status == FILE_CONT)
        return_status = progress_update_one (tctx, ctx, file_size);

  ret_fast:
    vfs_path_free (src_vpath);
    vfs_path_free (dst_vpath);
    return return_status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * I think these copy_*_* functions should have a return type.
 * anyway, this function *must* have two directories as arguments.
 */
/* FIXME: This function needs to check the return values of the
   function calls */

FileProgressStatus
copy_dir_dir (FileOpTotalContext * tctx, FileOpContext * ctx, const char *s, const char *d,
              gboolean toplevel, gboolean move_over, gboolean do_delete, GSList * parent_dirs)
{
    struct dirent *next;
    struct stat buf, cbuf;
    DIR *reading;
    char *dest_dir = NULL;
    FileProgressStatus return_status = FILE_CONT;
    struct utimbuf utb;
    struct link *lp;
    vfs_path_t *src_vpath, *dst_vpath, *dest_dir_vpath = NULL;
    gboolean do_mkdir = TRUE;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    /* First get the mode of the source dir */

  retry_src_stat:
    if ((*ctx->stat_func) (src_vpath, &cbuf) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Cannot stat source directory \"%s\"\n%s"), s);
            if (return_status == FILE_RETRY)
                goto retry_src_stat;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret_fast;
    }

    if (is_in_linklist (dest_dirs, src_vpath, &cbuf))
    {
        /* Don't copy a directory we created before (we don't want to copy 
           infinitely if a directory is copied into itself) */
        /* FIXME: should there be an error message and FILE_SKIP? - Norbert */
        return_status = FILE_CONT;
        goto ret_fast;
    }

    /* Hmm, hardlink to directory??? - Norbert */
    /* FIXME: In this step we should do something
       in case the destination already exist */
    /* Check the hardlinks */
    if (ctx->preserve && cbuf.st_nlink > 1 && check_hardlinks (src_vpath, dst_vpath, &cbuf))
    {
        /* We have made a hardlink - no more processing is necessary */
        goto ret_fast;
    }

    if (!S_ISDIR (cbuf.st_mode))
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (_("Source \"%s\" is not a directory\n%s"), s);
            if (return_status == FILE_RETRY)
                goto retry_src_stat;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret_fast;
    }

    if (is_in_linklist (parent_dirs, src_vpath, &cbuf))
    {
        /* we found a cyclic symbolic link */
        message (D_ERROR, MSG_ERROR, _("Cannot copy cyclic symbolic link\n\"%s\""), s);
        return_status = FILE_SKIP;
        goto ret_fast;
    }

    lp = g_new0 (struct link, 1);
    lp->vfs = vfs_path_get_by_index (src_vpath, -1)->class;
    lp->ino = cbuf.st_ino;
    lp->dev = cbuf.st_dev;
    parent_dirs = g_slist_prepend (parent_dirs, lp);

  retry_dst_stat:
    /* Now, check if the dest dir exists, if not, create it. */
    if (mc_stat (dst_vpath, &buf) != 0)
    {
        /* Here the dir doesn't exist : make it ! */
        if (move_over)
        {
            if (mc_rename (src_vpath, dst_vpath) == 0)
            {
                return_status = FILE_CONT;
                goto ret;
            }
        }
        dest_dir = g_strdup (d);
    }
    else
    {
        /*
         * If the destination directory exists, we want to copy the whole
         * directory, but we only want this to happen once.
         *
         * Escape sequences added to the * to compiler warnings.
         * so, say /bla exists, if we copy /tmp/\* to /bla, we get /bla/tmp/\*
         * or ( /bla doesn't exist )       /tmp/\* to /bla     ->  /bla/\*
         */
        if (!S_ISDIR (buf.st_mode))
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status = file_error (_("Destination \"%s\" must be a directory\n%s"), d);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                if (return_status == FILE_RETRY)
                    goto retry_dst_stat;
            }
            goto ret;
        }
        /* Dive into subdir if exists */
        if (toplevel && ctx->dive_into_subdirs)
            dest_dir = mc_build_filename (d, x_basename (s), NULL);
        else
        {
            dest_dir = g_strdup (d);
            do_mkdir = FALSE;
        }
    }

    dest_dir_vpath = vfs_path_from_str (dest_dir);

    if (do_mkdir)
    {
        while (my_mkdir (dest_dir_vpath, (cbuf.st_mode & ctx->umask_kill) | S_IRWXU) != 0)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (_("Cannot create target directory \"%s\"\n%s"), dest_dir);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
            }
            if (return_status != FILE_RETRY)
                goto ret;
        }

        lp = g_new0 (struct link, 1);
        mc_stat (dest_dir_vpath, &buf);
        lp->vfs = vfs_path_get_by_index (dest_dir_vpath, -1)->class;
        lp->ino = buf.st_ino;
        lp->dev = buf.st_dev;
        dest_dirs = g_slist_prepend (dest_dirs, lp);
    }

    if (ctx->preserve_uidgid)
    {
        while (mc_chown (dest_dir_vpath, cbuf.st_uid, cbuf.st_gid) != 0)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (_("Cannot chown target directory \"%s\"\n%s"), dest_dir);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
            }
            if (return_status != FILE_RETRY)
                goto ret;
        }
    }

    /* open the source dir for reading */
    reading = mc_opendir (src_vpath);
    if (reading == NULL)
        goto ret;

    while ((next = mc_readdir (reading)) && return_status != FILE_ABORT)
    {
        char *path;
        vfs_path_t *tmp_vpath;
        /*
         * Now, we don't want '.' and '..' to be created / copied at any time
         */
        if (!strcmp (next->d_name, "."))
            continue;
        if (!strcmp (next->d_name, ".."))
            continue;

        /* get the filename and add it to the src directory */
        path = mc_build_filename (s, next->d_name, NULL);
        tmp_vpath = vfs_path_from_str (path);

        (*ctx->stat_func) (tmp_vpath, &buf);
        if (S_ISDIR (buf.st_mode))
        {
            char *mdpath;

            mdpath = mc_build_filename (dest_dir, next->d_name, NULL);
            /*
             * From here, we just intend to recursively copy subdirs, not
             * the double functionality of copying different when the target
             * dir already exists. So, we give the recursive call the flag 0
             * meaning no toplevel.
             */
            return_status =
                copy_dir_dir (tctx, ctx, path, mdpath, FALSE, FALSE, do_delete, parent_dirs);
            g_free (mdpath);
        }
        else
        {
            char *dest_file;

            dest_file = mc_build_filename (dest_dir, x_basename (path), NULL);
            return_status = copy_file_file (tctx, ctx, path, dest_file);
            g_free (dest_file);
        }
        if (do_delete && return_status == FILE_CONT)
        {
            if (ctx->erase_at_end)
            {
                lp = g_new0 (struct link, 1);
                lp->src_vpath = vfs_path_clone (tmp_vpath);
                lp->st_mode = buf.st_mode;
                erase_list = g_slist_append (erase_list, lp);
            }
            else if (S_ISDIR (buf.st_mode))
                return_status = erase_dir_iff_empty (ctx, path);
            else
                return_status = erase_file (tctx, ctx, tmp_vpath);
        }
        g_free (path);
        vfs_path_free (tmp_vpath);
    }
    mc_closedir (reading);

    if (ctx->preserve)
    {
        mc_chmod (dest_dir_vpath, cbuf.st_mode & ctx->umask_kill);
        utb.actime = cbuf.st_atime;
        utb.modtime = cbuf.st_mtime;
        mc_utime (dest_dir_vpath, &utb);
    }
    else
    {
        cbuf.st_mode = umask (-1);
        umask (cbuf.st_mode);
        cbuf.st_mode = 0100777 & ~cbuf.st_mode;
        mc_chmod (dest_dir_vpath, cbuf.st_mode & ctx->umask_kill);
    }

  ret:
    g_free (dest_dir);
    vfs_path_free (dest_dir_vpath);
    free_link (parent_dirs->data);
    g_slist_free_1 (parent_dirs);
  ret_fast:
    vfs_path_free (src_vpath);
    vfs_path_free (dst_vpath);
    return return_status;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Move routines */

FileProgressStatus
move_dir_dir (FileOpTotalContext * tctx, FileOpContext * ctx, const char *s, const char *d)
{
    struct stat sbuf, dbuf, destbuf;
    struct link *lp;
    char *destdir;
    FileProgressStatus return_status;
    gboolean move_over = FALSE;
    gboolean dstat_ok;
    vfs_path_t *src_vpath, *dst_vpath, *destdir_vpath;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    file_progress_show_source (ctx, src_vpath);
    file_progress_show_target (ctx, dst_vpath);

    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret_fast;
    }

    mc_refresh ();

    mc_stat (src_vpath, &sbuf);

    dstat_ok = (mc_stat (dst_vpath, &dbuf) == 0);
    if (dstat_ok && sbuf.st_dev == dbuf.st_dev && sbuf.st_ino == dbuf.st_ino)
    {
        return_status = warn_same_file (_("\"%s\"\nand\n\"%s\"\nare the same directory"), s, d);
        goto ret_fast;
    }

    if (!dstat_ok)
        destdir = g_strdup (d); /* destination doesn't exist */
    else if (!ctx->dive_into_subdirs)
    {
        destdir = g_strdup (d);
        move_over = TRUE;
    }
    else
        destdir = mc_build_filename (d, x_basename (s), NULL);

    destdir_vpath = vfs_path_from_str (destdir);

    /* Check if the user inputted an existing dir */
  retry_dst_stat:
    if (mc_stat (destdir_vpath, &destbuf) == 0)
    {
        if (move_over)
        {
            return_status = copy_dir_dir (tctx, ctx, s, destdir, FALSE, TRUE, TRUE, NULL);

            if (return_status != FILE_CONT)
                goto ret;
            goto oktoret;
        }
        else if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            if (S_ISDIR (destbuf.st_mode))
                return_status = file_error (_("Cannot overwrite directory \"%s\"\n%s"), destdir);
            else
                return_status = file_error (_("Cannot overwrite file \"%s\"\n%s"), destdir);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_dst_stat;
        }

        g_free (destdir);
        vfs_path_free (destdir_vpath);
        goto ret_fast;
    }

  retry_rename:
    if (mc_rename (src_vpath, destdir_vpath) == 0)
    {
        return_status = FILE_CONT;
        goto ret;
    }

    if (errno != EXDEV)
    {
        if (!ctx->skip_all)
        {
            return_status = files_error (_("Cannot move directory \"%s\" to \"%s\"\n%s"), s, d);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_rename;
        }
        goto ret;
    }
    /* Failed because of filesystem boundary -> copy dir instead */
    return_status = copy_dir_dir (tctx, ctx, s, destdir, FALSE, FALSE, TRUE, NULL);

    if (return_status != FILE_CONT)
        goto ret;
  oktoret:
    file_progress_show_source (ctx, NULL);
    file_progress_show (ctx, 0, 0, "", FALSE);

    return_status = check_progress_buttons (ctx);
    if (return_status != FILE_CONT)
        goto ret;

    mc_refresh ();
    if (ctx->erase_at_end)
    {
        for (; erase_list != NULL && return_status != FILE_ABORT;)
        {
            lp = (struct link *) erase_list->data;

            if (S_ISDIR (lp->st_mode))
            {
                char *src_path;

                src_path = vfs_path_to_str (lp->src_vpath);
                return_status = erase_dir_iff_empty (ctx, src_path);
                g_free (src_path);
            }
            else
                return_status = erase_file (tctx, ctx, lp->src_vpath);

            erase_list = g_slist_remove (erase_list, lp);
            free_link (lp);
        }
    }
    erase_dir_iff_empty (ctx, s);

  ret:
    g_free (destdir);
    vfs_path_free (destdir_vpath);
    erase_list = free_linklist (erase_list);
  ret_fast:
    vfs_path_free (src_vpath);
    vfs_path_free (dst_vpath);
    return return_status;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Erase routines */

FileProgressStatus
erase_dir (FileOpTotalContext * tctx, FileOpContext * ctx, const vfs_path_t * s_vpath)
{
    FileProgressStatus error;
    char *s;

    s = vfs_path_to_str (s_vpath);

    /*
       if (strcmp (s, "..") == 0)
       return FILE_SKIP;

       if (strcmp (s, ".") == 0)
       return FILE_SKIP;
     */

    file_progress_show_deleting (ctx, s);
    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        g_free (s);
        return FILE_ABORT;
    }
    mc_refresh ();

    /* The old way to detect a non empty directory was:
       error = my_rmdir (s);
       if (error && (errno == ENOTEMPTY || errno == EEXIST))){
       For the linux user space nfs server (nfs-server-2.2beta29-2)
       we would have to check also for EIO. I hope the new way is
       fool proof. (Norbert)
     */
    error = check_dir_is_empty (s_vpath);
    if (error == 0)
    {                           /* not empty */
        error = query_recursive (ctx, s);
        if (error == FILE_CONT)
            error = recursive_erase (tctx, ctx, s);
        g_free (s);
        return error;
    }

    while (my_rmdir (s) == -1 && !ctx->skip_all)
    {
        error = file_error (_("Cannot remove directory \"%s\"\n%s"), s);
        if (error != FILE_RETRY)
        {
            g_free (s);
            return error;
        }
    }

    g_free (s);
    return FILE_CONT;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Panel operate routines */

ComputeDirSizeUI *
compute_dir_size_create_ui (gboolean allow_skip)
{
    ComputeDirSizeUI *ui;

    const char *b1_name = N_("&Abort");
    const char *b2_name = N_("&Skip");
    int b1_width, b2_width = 0, b_width = 0;
    int b1_x;
    int ui_width;
    Widget *b;

#ifdef ENABLE_NLS
    b1_name = _(b1_name);
    b2_name = _(b2_name);
#endif

    b1_width = str_term_width1 (b1_name) + 4;
    if (allow_skip)
        b2_width = str_term_width1 (b2_name) + 4 + 1;
    b_width = b1_width + b2_width;

    ui = g_new (ComputeDirSizeUI, 1);

    ui_width = max (COLS / 2, b_width + 6);
    ui->dlg = create_dlg (TRUE, 0, 0, 8, ui_width, dialog_colors, NULL, NULL, NULL,
                          _("Directory scanning"), DLG_CENTER);

    ui->dirname = label_new (2, 3, "");
    add_widget (ui->dlg, ui->dirname);
    add_widget (ui->dlg, hline_new (4, -1, -1));
    b1_x = (ui_width - b_width) / 2;
    b = WIDGET (button_new (5, b1_x, FILE_ABORT, NORMAL_BUTTON, b1_name, NULL));
    add_widget (ui->dlg, b);
    if (allow_skip)
    {
        add_widget (ui->dlg,
                    button_new (5, b1_x + 1 + b1_width, FILE_SKIP, NORMAL_BUTTON, b2_name, NULL));
        dlg_select_widget (b);
    }

    /* We will manage the dialog without any help,
       that's why we have to call init_dlg */
    init_dlg (ui->dlg);

    return ui;
}

/* --------------------------------------------------------------------------------------------- */

void
compute_dir_size_destroy_ui (ComputeDirSizeUI * ui)
{
    if (ui != NULL)
    {
        /* schedule to update passive panel */
        other_panel->dirty = 1;

        /* close and destroy dialog */
        dlg_run_done (ui->dlg);
        destroy_dlg (ui->dlg);
        g_free (ui);
    }
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
compute_dir_size_update_ui (void *ui, const vfs_path_t * dirname_vpath, size_t dir_count,
                            uintmax_t total_size)
{
    ComputeDirSizeUI *this = (ComputeDirSizeUI *) ui;
    int c;
    Gpm_Event event;
    char *dirname;
    char buffer[BUF_1K];

    if (ui == NULL)
        return FILE_CONT;

    dirname = vfs_path_to_str (dirname_vpath);
    g_snprintf (buffer, sizeof (buffer), _("%s\nDirectories: %zd, total size: %s"),
                str_trunc (dirname, WIDGET (this->dlg)->cols - 6), dir_count,
                size_trunc_sep (total_size, panels_options.kilobyte_si));
    g_free (dirname);
    label_set_text (this->dirname, buffer);

    event.x = -1;               /* Don't show the GPM cursor */
    c = tty_get_event (&event, FALSE, FALSE);
    if (c == EV_NONE)
        return FILE_CONT;

    /* Reinitialize to avoid old values after events other than
       selecting a button */
    this->dlg->ret_value = FILE_CONT;

    dlg_process_event (this->dlg, c, &event);

    switch (this->dlg->ret_value)
    {
    case B_CANCEL:
    case FILE_ABORT:
        return FILE_ABORT;
    case FILE_SKIP:
        return FILE_SKIP;
    default:
        return FILE_CONT;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * compute_dir_size:
 *
 * Computes the number of bytes used by the files in a directory
 */

FileProgressStatus
compute_dir_size (const vfs_path_t * dirname_vpath, void *ui, compute_dir_size_callback cback,
                  size_t * ret_marked, uintmax_t * ret_total, gboolean compute_symlinks)
{
    size_t dir_count = 0;

    return do_compute_dir_size (dirname_vpath, ui, cback, &dir_count, ret_marked, ret_total,
                                compute_symlinks);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * panel_operate:
 *
 * Performs one of the operations on the selection on the source_panel
 * (copy, delete, move).
 *
 * Returns TRUE if did change the directory
 * structure, Returns FALSE if user aborted
 *
 * force_single forces operation on the current entry and affects
 * default destination.  Current filename is used as default.
 */

gboolean
panel_operate (void *source_panel, FileOperation operation, gboolean force_single)
{
    WPanel *panel = (WPanel *) source_panel;
    const gboolean single_entry = force_single || (panel->marked <= 1)
        || (get_current_type () == view_tree);

    char *source = NULL;
#ifdef WITH_FULL_PATHS
    vfs_path_t *source_with_vpath = NULL;
    char *source_with_path_str = NULL;
#else
#define source_with_path source
#endif /* !WITH_FULL_PATHS */
    char *dest = NULL;
    vfs_path_t *dest_vpath = NULL;
    char *temp = NULL;
    char *save_cwd = NULL, *save_dest = NULL;
    struct stat src_stat;
    gboolean ret_val = TRUE;
    int i;
    FileProgressStatus value;
    FileOpContext *ctx;
    FileOpTotalContext *tctx;
    vfs_path_t *tmp_vpath;
    filegui_dialog_type_t dialog_type = FILEGUI_DIALOG_ONE_ITEM;

    gboolean do_bg = FALSE;     /* do background operation? */

    static gboolean i18n_flag = FALSE;
    if (!i18n_flag)
    {
        for (i = sizeof (op_names) / sizeof (op_names[0]); i--;)
            op_names[i] = Q_ (op_names[i]);
        i18n_flag = TRUE;
    }

    linklist = free_linklist (linklist);
    dest_dirs = free_linklist (dest_dirs);

    if (single_entry)
    {
        vfs_path_t *source_vpath;

        if (force_single)
            source = g_strdup (selection (panel)->fname);
        else
            source = panel_get_file (panel);

        if (strcmp (source, "..") == 0)
        {
            g_free (source);
            message (D_ERROR, MSG_ERROR, _("Cannot operate on \"..\"!"));
            return FALSE;
        }

        source_vpath = vfs_path_from_str (source);
        /* Update stat to get actual info */
        if (mc_lstat (source_vpath, &src_stat) != 0)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot stat \"%s\"\n%s"),
                     path_trunc (source, 30), unix_error_string (errno));

            /* Directory was changed outside MC. Reload it forced */
            if (!panel->is_panelized)
            {
                panel_update_flags_t flags = UP_RELOAD;

                /* don't update panelized panel */
                if (get_other_type () == view_listing && other_panel->is_panelized)
                    flags |= UP_ONLY_CURRENT;

                update_panels (flags, UP_KEEPSEL);
            }
            vfs_path_free (source_vpath);
            return FALSE;
        }
        vfs_path_free (source_vpath);
    }

    ctx = file_op_context_new (operation);

    /* Show confirmation dialog */
    if (operation != OP_DELETE)
    {
        char *tmp_dest_dir, *dest_dir;
        char *format;

        /* Forced single operations default to the original name */
        if (force_single)
            tmp_dest_dir = g_strdup (source);
        else if (get_other_type () == view_listing)
            tmp_dest_dir = vfs_path_to_str (other_panel->cwd_vpath);
        else
            tmp_dest_dir = vfs_path_to_str (panel->cwd_vpath);
        /*
         * Add trailing backslash only when do non-local ops.
         * It saves user from occasional file renames (when destination
         * dir is deleted)
         */
        if (!force_single && tmp_dest_dir[0] != '\0'
            && tmp_dest_dir[strlen (tmp_dest_dir) - 1] != PATH_SEP)
        {
            /* add trailing separator */
            dest_dir = g_strconcat (tmp_dest_dir, PATH_SEP_STR, (char *) NULL);
            g_free (tmp_dest_dir);
        }
        else
        {
            /* just copy */
            dest_dir = tmp_dest_dir;
        }
        if (dest_dir == NULL)
        {
            ret_val = FALSE;
            goto ret_fast;
        }

        /* Generate confirmation prompt */
        format = panel_operate_generate_prompt (panel, operation, source != NULL, &src_stat);

        dest = file_mask_dialog (ctx, operation, source != NULL, format,
                                 source != NULL ? (void *) source
                                 : (void *) &panel->marked, dest_dir, &do_bg);

        g_free (format);
        g_free (dest_dir);

        if (dest == NULL || dest[0] == '\0')
        {
            g_free (dest);
            ret_val = FALSE;
            goto ret_fast;
        }
        dest_vpath = vfs_path_from_str (dest);
    }
    else if (confirm_delete)
    {
        char *format;
        char fmd_buf[BUF_MEDIUM];

        /* Generate confirmation prompt */
        format = panel_operate_generate_prompt (panel, OP_DELETE, source != NULL, &src_stat);

        if (source == NULL)
            g_snprintf (fmd_buf, sizeof (fmd_buf), format, panel->marked);
        else
        {
            const int fmd_xlen = 64;
            i = fmd_xlen - str_term_width1 (format) - 4;
            g_snprintf (fmd_buf, sizeof (fmd_buf), format, str_trunc (source, i));
        }

        g_free (format);

        if (safe_delete)
            query_set_sel (1);

        i = query_dialog (op_names[operation], fmd_buf, D_ERROR, 2, _("&Yes"), _("&No"));

        if (i != 0)
        {
            ret_val = FALSE;
            goto ret_fast;
        }
    }

    tctx = file_op_total_context_new ();
    gettimeofday (&tctx->transfer_start, (struct timezone *) NULL);

#ifdef ENABLE_BACKGROUND
    /* Did the user select to do a background operation? */
    if (do_bg)
    {
        int v;
        char *cwd_str;

        cwd_str = vfs_path_to_str (panel->cwd_vpath);
        v = do_background (ctx, g_strconcat (op_names[operation], ": ", cwd_str, (char *) NULL));
        g_free (cwd_str);
        if (v == -1)
            message (D_ERROR, MSG_ERROR, _("Sorry, I could not put the job in background"));

        /* If we are the parent */
        if (v == 1)
        {
            mc_setctl (panel->cwd_vpath, VFS_SETCTL_FORGET, NULL);

            mc_setctl (dest_vpath, VFS_SETCTL_FORGET, NULL);
            vfs_path_free (dest_vpath);
            g_free (dest);
            /*          file_op_context_destroy (ctx); */
            return FALSE;
        }
    }
    else
#endif /* ENABLE_BACKGROUND */
    {
        if (operation == OP_DELETE)
            dialog_type = FILEGUI_DIALOG_DELETE_ITEM;
        else
        {
            dialog_type = ((operation != OP_COPY) || single_entry || force_single)
                ? FILEGUI_DIALOG_ONE_ITEM : FILEGUI_DIALOG_MULTI_ITEM;

            if (single_entry && (operation == OP_COPY) && S_ISDIR (selection (panel)->st.st_mode))
                dialog_type = FILEGUI_DIALOG_MULTI_ITEM;
        }
    }

    /* Initialize things */
    /* We do not want to trash cache every time file is
       created/touched. However, this will make our cache contain
       invalid data. */
    if ((dest != NULL)
        && (mc_setctl (dest_vpath, VFS_SETCTL_STALE_DATA, GUINT_TO_POINTER (1)) != 0))
        save_dest = g_strdup (dest);

    if ((vfs_path_tokens_count (panel->cwd_vpath) != 0)
        && (mc_setctl (panel->cwd_vpath, VFS_SETCTL_STALE_DATA, GUINT_TO_POINTER (1)) != 0))
        save_cwd = vfs_path_to_str (panel->cwd_vpath);

    /* Now, let's do the job */

    /* This code is only called by the tree and panel code */
    if (single_entry)
    {
        /* We now have ETA in all cases */

        /* One file: FIXME mc_chdir will take user out of any vfs */
        if ((operation != OP_COPY) && (get_current_type () == view_tree))
        {
            vfs_path_t *vpath;
            int chdir_retcode;

            vpath = vfs_path_from_str (PATH_SEP_STR);
            chdir_retcode = mc_chdir (vpath);
            vfs_path_free (vpath);
            if (chdir_retcode < 0)
            {
                ret_val = FALSE;
                goto clean_up;
            }
        }

        /* The source and src_stat variables have been initialized before */
#ifdef WITH_FULL_PATHS
        if (g_path_is_absolute (source))
            source_with_vpath = vfs_path_from_str (source);
        else
            source_with_vpath = vfs_path_append_new (panel->cwd_vpath, source, (char *) NULL);
        source_with_path_str = vfs_path_to_str (source_with_vpath);
#endif /* WITH_FULL_PATHS */
        if (panel_operate_init_totals (operation, panel, source_with_path_str, ctx, dialog_type) ==
            FILE_CONT)
        {
            if (operation == OP_DELETE)
            {
                if (S_ISDIR (src_stat.st_mode))
                    value = erase_dir (tctx, ctx, source_with_vpath);
                else
                    value = erase_file (tctx, ctx, source_with_vpath);
            }
            else
            {
                temp = transform_source (ctx, source_with_path_str);
                if (temp == NULL)
                    value = transform_error;
                else
                {
                    char *repl_dest, *temp2;

                    repl_dest = mc_search_prepare_replace_str2 (ctx->search_handle, dest);
                    if (ctx->search_handle->error != MC_SEARCH_E_OK)
                    {
                        message (D_ERROR, MSG_ERROR, "%s", ctx->search_handle->error_str);
                        g_free (repl_dest);
                        goto clean_up;
                    }

                    temp2 = mc_build_filename (repl_dest, temp, NULL);
                    g_free (temp);
                    g_free (repl_dest);
                    g_free (dest);
                    vfs_path_free (dest_vpath);
                    dest = temp2;
                    dest_vpath = vfs_path_from_str (dest);

                    switch (operation)
                    {
                    case OP_COPY:
                        /* we use file_mask_op_follow_links only with OP_COPY */
                        ctx->stat_func (source_with_vpath, &src_stat);

                        if (S_ISDIR (src_stat.st_mode))
                            value = copy_dir_dir (tctx, ctx, source_with_path_str, dest,
                                                  TRUE, FALSE, FALSE, NULL);
                        else
                            value = copy_file_file (tctx, ctx, source_with_path_str, dest);
                        break;

                    case OP_MOVE:
                        if (S_ISDIR (src_stat.st_mode))
                            value = move_dir_dir (tctx, ctx, source_with_path_str, dest);
                        else
                            value = move_file_file (tctx, ctx, source_with_path_str, dest);
                        break;

                    default:
                        /* Unknown file operation */
                        abort ();
                    }
                }
            }                   /* Copy or move operation */

            if ((value == FILE_CONT) && !force_single)
                unmark_files (panel);
        }
    }
    else
    {
        /* Many files */

        /* Check destination for copy or move operation */
        while (operation != OP_DELETE)
        {
            int dst_result;
            struct stat dst_stat;

            dst_result = mc_stat (dest_vpath, &dst_stat);

            if ((dst_result != 0) || S_ISDIR (dst_stat.st_mode))
                break;

            if (ctx->skip_all
                || file_error (_("Destination \"%s\" must be a directory\n%s"), dest) != FILE_RETRY)
                goto clean_up;
        }

        if (panel_operate_init_totals (operation, panel, NULL, ctx, dialog_type) == FILE_CONT)
        {
            /* Loop for every file, perform the actual copy operation */
            for (i = 0; i < panel->count; i++)
            {
                const char *source2;

                if (!panel->dir.list[i].f.marked)
                    continue;   /* Skip the unmarked ones */

                source2 = panel->dir.list[i].fname;
                src_stat = panel->dir.list[i].st;

#ifdef WITH_FULL_PATHS
                g_free (source_with_path_str);
                vfs_path_free (source_with_vpath);
                if (g_path_is_absolute (source2))
                    source_with_vpath = vfs_path_from_str (source2);
                else
                    source_with_vpath =
                        vfs_path_append_new (panel->cwd_vpath, source2, (char *) NULL);
                source_with_path_str = vfs_path_to_str (source_with_vpath);
#endif /* WITH_FULL_PATHS */

                if (operation == OP_DELETE)
                {
                    if (S_ISDIR (src_stat.st_mode))
                        value = erase_dir (tctx, ctx, source_with_vpath);
                    else
                        value = erase_file (tctx, ctx, source_with_vpath);
                }
                else
                {
                    temp = transform_source (ctx, source_with_path_str);
                    if (temp == NULL)
                        value = transform_error;
                    else
                    {
                        char *temp2, *temp3, *repl_dest;

                        repl_dest = mc_search_prepare_replace_str2 (ctx->search_handle, dest);
                        if (ctx->search_handle->error != MC_SEARCH_E_OK)
                        {
                            message (D_ERROR, MSG_ERROR, "%s", ctx->search_handle->error_str);
                            g_free (repl_dest);
                            goto clean_up;
                        }

                        temp2 = mc_build_filename (repl_dest, temp, NULL);
                        g_free (temp);
                        g_free (repl_dest);
                        temp3 = source_with_path_str;
                        source_with_path_str = strutils_shell_unescape (source_with_path_str);
                        g_free (temp3);
                        temp3 = temp2;
                        temp2 = strutils_shell_unescape (temp2);
                        g_free (temp3);

                        switch (operation)
                        {
                        case OP_COPY:
                            /* we use file_mask_op_follow_links only with OP_COPY */
                            {
                                vfs_path_t *vpath;

                                vpath = vfs_path_from_str (source_with_path_str);
                                ctx->stat_func (vpath, &src_stat);
                                vfs_path_free (vpath);
                            }
                            if (S_ISDIR (src_stat.st_mode))
                                value = copy_dir_dir (tctx, ctx, source_with_path_str, temp2,
                                                      TRUE, FALSE, FALSE, NULL);
                            else
                                value = copy_file_file (tctx, ctx, source_with_path_str, temp2);
                            dest_dirs = free_linklist (dest_dirs);
                            break;

                        case OP_MOVE:
                            if (S_ISDIR (src_stat.st_mode))
                                value = move_dir_dir (tctx, ctx, source_with_path_str, temp2);
                            else
                                value = move_file_file (tctx, ctx, source_with_path_str, temp2);
                            break;

                        default:
                            /* Unknown file operation */
                            abort ();
                        }

                        g_free (temp2);
                    }
                }               /* Copy or move operation */

                if (value == FILE_ABORT)
                    break;

                if (value == FILE_CONT)
                    do_file_mark (panel, i, 0);

                if (verbose && ctx->dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
                {
                    file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
                    file_progress_show_total (tctx, ctx, tctx->progress_bytes, FALSE);
                }

                if (operation != OP_DELETE)
                    file_progress_show (ctx, 0, 0, "", FALSE);

                if (check_progress_buttons (ctx) == FILE_ABORT)
                    break;

                mc_refresh ();
            }                   /* Loop for every file */
        }
    }                           /* Many entries */

  clean_up:
    /* Clean up */
    if (save_cwd != NULL)
    {
        tmp_vpath = vfs_path_from_str (save_cwd);
        mc_setctl (tmp_vpath, VFS_SETCTL_STALE_DATA, NULL);
        vfs_path_free (tmp_vpath);
        g_free (save_cwd);
    }

    if (save_dest != NULL)
    {
        tmp_vpath = vfs_path_from_str (save_dest);
        mc_setctl (tmp_vpath, VFS_SETCTL_STALE_DATA, NULL);
        vfs_path_free (tmp_vpath);
        g_free (save_dest);
    }

    linklist = free_linklist (linklist);
    dest_dirs = free_linklist (dest_dirs);
#ifdef WITH_FULL_PATHS
    g_free (source_with_path_str);
    vfs_path_free (source_with_vpath);
#endif /* WITH_FULL_PATHS */
    g_free (dest);
    vfs_path_free (dest_vpath);
    g_free (ctx->dest_mask);
    ctx->dest_mask = NULL;

#ifdef ENABLE_BACKGROUND
    /* Let our parent know we are saying bye bye */
    if (mc_global.we_are_background)
    {
        int cur_pid = getpid ();
        /* Send pid to parent with child context, it is fork and
           don't modify real parent ctx */
        ctx->pid = cur_pid;
        parent_call ((void *) end_bg_process, ctx, 0);

        vfs_shut ();
        my_exit (EXIT_SUCCESS);
    }
#endif /* ENABLE_BACKGROUND */

    file_op_total_context_destroy (tctx);
  ret_fast:
    file_op_context_destroy (ctx);
    g_free (source);

    return ret_val;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Query/status report routines */
/** Report error with one file */
FileProgressStatus
file_error (const char *format, const char *file)
{
    char buf[BUF_MEDIUM];

    g_snprintf (buf, sizeof (buf), format, path_trunc (file, 30), unix_error_string (errno));

    return do_file_error (buf);
}

/* --------------------------------------------------------------------------------------------- */

/*
   Cause emacs to enter folding mode for this file:
   Local variables:
   end:
 */
