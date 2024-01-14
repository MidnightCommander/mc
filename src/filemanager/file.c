/*
   File management.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1995
   Fred Leeflang, 1994, 1995
   Miguel de Icaza, 1994, 1995, 1996
   Jakub Jelinek, 1995, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Andrew Borodin <aborodin@vmail.ru>, 2011-2022

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

/* Needed for other_panel and WTree */
#include "dir.h"
#include "filegui.h"
#include "filenot.h"
#include "tree.h"
#include "filemanager.h"        /* other_panel */
#include "layout.h"             /* rotate_dash() */
#include "ioblksize.h"          /* io_blksize() */

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

#define FILEOP_UPDATE_INTERVAL 2
#define FILEOP_STALLING_INTERVAL 4
#define FILEOP_UPDATE_INTERVAL_US (FILEOP_UPDATE_INTERVAL * G_USEC_PER_SEC)
#define FILEOP_STALLING_INTERVAL_US (FILEOP_STALLING_INTERVAL * G_USEC_PER_SEC)

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
    DEST_NONE = 0,              /**< Not created */
    DEST_SHORT_QUERY,           /**< Created, not fully copied, query to do */
    DEST_SHORT_KEEP,            /**< Created, not fully copied, keep it */
    DEST_SHORT_DELETE,          /**< Created, not fully copied, delete it */
    DEST_FULL                   /**< Created, fully copied */
} dest_status_t;

/* Status of hard link creation */
typedef enum
{
    HARDLINK_OK = 0,            /**< Hardlink was created successfully */
    HARDLINK_CACHED,            /**< Hardlink was added to the cache */
    HARDLINK_NOTLINK,           /**< This is not a hard link */
    HARDLINK_UNSUPPORTED,       /**< VFS doesn't support hard links */
    HARDLINK_ERROR,             /**< Hard link creation error */
    HARDLINK_ABORT              /**< Stop file operation after hardlink creation error */
} hardlink_status_t;

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
 * %n - the '\n' symbol to form two-line prompt for delete or space for other operations
 */
/* xgettext:no-c-format */
static const char *one_format = N_("%o %f%n\"%s\"%m");
/* xgettext:no-c-format */
static const char *many_format = N_("%o %d %f%m");

static const char *prompt_parts[] = {
    N_("file"),
    N_("files"),
    N_("directory"),
    N_("directories"),
    N_("files/directories"),
    /* TRANSLATORS: keep leading space here to split words in Copy/Move dialog */
    N_(" with source mask:")
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* the hard link cache */
static GSList *linklist = NULL;

/* the files-to-be-erased list */
static GQueue *erase_list = NULL;

/*
 * In copy_dir_dir we use two additional single linked lists: The first -
 * variable name 'parent_dirs' - holds information about already copied
 * directories and is used to detect cyclic symbolic links.
 * The second ('dest_dirs' below) holds information about just created
 * target directories and is used to detect when an directory is copied
 * into itself (we don't want to copy infinitely).
 * Both lists don't use the linkcount and name structure members of struct
 * link.
 */
static GSList *dest_dirs = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
dirsize_status_locate_buttons (dirsize_status_msg_t * dsm)
{
    status_msg_t *sm = STATUS_MSG (dsm);
    Widget *wd = WIDGET (sm->dlg);
    int y, x;
    WRect r;

    y = wd->rect.y + 5;
    x = wd->rect.x;

    if (!dsm->allow_skip)
    {
        /* single button: "Abort" */
        x += (wd->rect.cols - dsm->abort_button->rect.cols) / 2;
        r = dsm->abort_button->rect;
        r.y = y;
        r.x = x;
        widget_set_size_rect (dsm->abort_button, &r);
    }
    else
    {
        /* two buttons: "Abort" and "Skip" */
        int cols;

        cols = dsm->abort_button->rect.cols + dsm->skip_button->rect.cols + 1;
        x += (wd->rect.cols - cols) / 2;
        r = dsm->abort_button->rect;
        r.y = y;
        r.x = x;
        widget_set_size_rect (dsm->abort_button, &r);
        x += dsm->abort_button->rect.cols + 1;
        r = dsm->skip_button->rect;
        r.y = y;
        r.x = x;
        widget_set_size_rect (dsm->skip_button, &r);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
build_dest (file_op_context_t * ctx, const char *src, const char *dest, FileProgressStatus * status)
{
    char *s, *q;
    const char *fnsource;

    *status = FILE_CONT;

    s = g_strdup (src);

    /* We remove \n from the filename since regex routines would use \n as an anchor */
    /* this is just to be allowed to maniupulate file names with \n on it */
    for (q = s; *q != '\0'; q++)
        if (*q == '\n')
            *q = ' ';

    fnsource = x_basename (s);

    if (!mc_search_run (ctx->search_handle, fnsource, 0, strlen (fnsource), NULL))
    {
        q = NULL;
        *status = FILE_SKIP;
    }
    else
    {
        q = mc_search_prepare_replace_str2 (ctx->search_handle, ctx->dest_mask);
        if (ctx->search_handle->error != MC_SEARCH_E_OK)
        {
            if (ctx->search_handle->error_str != NULL)
                message (D_ERROR, MSG_ERROR, "%s", ctx->search_handle->error_str);

            *status = FILE_ABORT;
        }
    }

    MC_PTR_FREE (s);

    if (*status == FILE_CONT)
    {
        char *repl_dest;

        repl_dest = mc_search_prepare_replace_str2 (ctx->search_handle, dest);
        if (ctx->search_handle->error == MC_SEARCH_E_OK)
            s = mc_build_filename (repl_dest, q, (char *) NULL);
        else
        {
            if (ctx->search_handle->error_str != NULL)
                message (D_ERROR, MSG_ERROR, "%s", ctx->search_handle->error_str);

            *status = FILE_ABORT;
        }

        g_free (repl_dest);
    }

    g_free (q);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static void
free_link (void *data)
{
    struct link *lp = (struct link *) data;

    vfs_path_free (lp->src_vpath, TRUE);
    vfs_path_free (lp->dst_vpath, TRUE);
    g_free (lp);
}

/* --------------------------------------------------------------------------------------------- */

static inline void *
free_erase_list (GQueue * lp)
{
    if (lp != NULL)
        g_queue_free_full (lp, free_link);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static inline void *
free_linklist (GSList * lp)
{
    g_slist_free_full (lp, free_link);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const struct link *
is_in_linklist (const GSList * lp, const vfs_path_t * vpath, const struct stat *sb)
{
    const struct vfs_class *class;
    ino_t ino = sb->st_ino;
    dev_t dev = sb->st_dev;

    class = vfs_path_get_last_path_vfs (vpath);

    for (; lp != NULL; lp = (const GSList *) g_slist_next (lp))
    {
        const struct link *lnk = (const struct link *) lp->data;

        if (lnk->vfs == class && lnk->ino == ino && lnk->dev == dev)
            return lnk;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check and made hardlink
 *
 * @return FALSE if the inode wasn't found in the cache and TRUE if it was found
 * and a hardlink was successfully made
 */

static hardlink_status_t
check_hardlinks (const vfs_path_t * src_vpath, const struct stat *src_stat,
                 const vfs_path_t * dst_vpath, gboolean * skip_all)
{
    struct link *lnk;
    ino_t ino = src_stat->st_ino;
    dev_t dev = src_stat->st_dev;

    if (src_stat->st_nlink < 2)
        return HARDLINK_NOTLINK;
    if ((vfs_file_class_flags (src_vpath) & VFSF_NOLINKS) != 0)
        return HARDLINK_UNSUPPORTED;

    lnk = (struct link *) is_in_linklist (linklist, src_vpath, src_stat);
    if (lnk != NULL)
    {
        int stat_result;
        struct stat link_stat;

        stat_result = mc_stat (lnk->src_vpath, &link_stat);

        if (stat_result == 0 && link_stat.st_ino == ino && link_stat.st_dev == dev)
        {
            const struct vfs_class *lp_name_class;
            const struct vfs_class *my_vfs;

            lp_name_class = vfs_path_get_last_path_vfs (lnk->src_vpath);
            my_vfs = vfs_path_get_last_path_vfs (src_vpath);

            if (lp_name_class == my_vfs)
            {
                const struct vfs_class *p_class, *dst_name_class;

                dst_name_class = vfs_path_get_last_path_vfs (dst_vpath);
                p_class = vfs_path_get_last_path_vfs (lnk->dst_vpath);

                if (dst_name_class == p_class)
                {
                    gboolean ok;

                    while (!(ok = (mc_stat (lnk->dst_vpath, &link_stat) == 0)) && !*skip_all)
                    {
                        FileProgressStatus status;

                        status =
                            file_error (TRUE, _("Cannot stat hardlink source file \"%s\"\n%s"),
                                        vfs_path_as_str (lnk->dst_vpath));
                        if (status == FILE_ABORT)
                            return HARDLINK_ABORT;
                        if (status == FILE_RETRY)
                            continue;
                        if (status == FILE_SKIPALL)
                            *skip_all = TRUE;
                        break;
                    }

                    /* if stat() finished unsuccessfully, don't try to create link */
                    if (!ok)
                        return HARDLINK_ERROR;

                    while (!(ok = (mc_link (lnk->dst_vpath, dst_vpath) == 0)) && !*skip_all)
                    {
                        FileProgressStatus status;

                        status =
                            file_error (TRUE, _("Cannot create target hardlink \"%s\"\n%s"),
                                        vfs_path_as_str (dst_vpath));
                        if (status == FILE_ABORT)
                            return HARDLINK_ABORT;
                        if (status == FILE_RETRY)
                            continue;
                        if (status == FILE_SKIPALL)
                            *skip_all = TRUE;
                        break;
                    }

                    /* Success? */
                    return (ok ? HARDLINK_OK : HARDLINK_ERROR);
                }
            }
        }

        if (!*skip_all)
        {
            FileProgressStatus status;

            /* Message w/o "Retry" action.
             *
             * FIXME: Can't say what errno is here. Define it and don't display.
             *
             * file_error() displays a message with text representation of errno
             * and the string passed to file_error() should provide the format "%s"
             * for that at end (see previous file_error() call for the reference).
             * But if format for errno isn't provided, it is safe, because C standard says:
             * "If the format is exhausted while arguments remain, the excess arguments
             * are evaluated (as always) but are otherwise ignored" (ISO/IEC 9899:1999,
             * section 7.19.6.1, paragraph 2).
             *
             */
            errno = 0;
            status =
                file_error (FALSE, _("Cannot create target hardlink \"%s\""),
                            vfs_path_as_str (dst_vpath));

            if (status == FILE_ABORT)
                return HARDLINK_ABORT;

            if (status == FILE_SKIPALL)
                *skip_all = TRUE;
        }

        return HARDLINK_ERROR;
    }

    lnk = g_try_new (struct link, 1);
    if (lnk != NULL)
    {
        lnk->vfs = vfs_path_get_last_path_vfs (src_vpath);
        lnk->ino = ino;
        lnk->dev = dev;
        lnk->linkcount = 0;
        lnk->st_mode = 0;
        lnk->src_vpath = vfs_path_clone (src_vpath);
        lnk->dst_vpath = vfs_path_clone (dst_vpath);

        linklist = g_slist_prepend (linklist, lnk);
    }

    return HARDLINK_CACHED;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Duplicate the contents of the symbolic link src_vpath in dst_vpath.
 * Try to make a stable symlink if the option "stable symlink" was
 * set in the file mask dialog.
 * If dst_path is an existing symlink it will be deleted silently
 * (upper levels take already care of existing files at dst_vpath).
 */

static FileProgressStatus
make_symlink (file_op_context_t * ctx, const vfs_path_t * src_vpath, const vfs_path_t * dst_vpath)
{
    const char *src_path;
    const char *dst_path;
    char link_target[MC_MAXPATHLEN];
    int len;
    FileProgressStatus return_status;
    struct stat dst_stat;
    gboolean dst_is_symlink;
    vfs_path_t *link_target_vpath = NULL;

    src_path = vfs_path_as_str (src_vpath);
    dst_path = vfs_path_as_str (dst_vpath);

    dst_is_symlink = (mc_lstat (dst_vpath, &dst_stat) == 0) && S_ISLNK (dst_stat.st_mode);

  retry_src_readlink:
    len = mc_readlink (src_vpath, link_target, sizeof (link_target) - 1);
    if (len < 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot read source link \"%s\"\n%s"), src_path);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_src_readlink;
        }
        goto ret;
    }

    link_target[len] = '\0';

    if (ctx->stable_symlinks && !(vfs_file_is_local (src_vpath) && vfs_file_is_local (dst_vpath)))
    {
        message (D_ERROR, MSG_ERROR,
                 _("Cannot make stable symlinks across "
                   "non-local filesystems:\n\nOption Stable Symlinks will be disabled"));
        ctx->stable_symlinks = FALSE;
    }

    if (ctx->stable_symlinks && !g_path_is_absolute (link_target))
    {
        const char *r;

        r = strrchr (src_path, PATH_SEP);
        if (r != NULL)
        {
            size_t slen;
            GString *p;
            vfs_path_t *q;

            slen = r - src_path + 1;

            p = g_string_sized_new (slen + len);
            g_string_append_len (p, src_path, slen);

            if (g_path_is_absolute (dst_path))
                q = vfs_path_from_str_flags (dst_path, VPF_NO_CANON);
            else
                q = vfs_path_build_filename (p->str, dst_path, (char *) NULL);

            if (vfs_path_tokens_count (q) > 1)
            {
                char *s = NULL;
                vfs_path_t *tmp_vpath1, *tmp_vpath2;

                g_string_append_len (p, link_target, len);
                tmp_vpath1 = vfs_path_vtokens_get (q, -1, 1);
                tmp_vpath2 = vfs_path_from_str (p->str);
                s = diff_two_paths (tmp_vpath1, tmp_vpath2);
                vfs_path_free (tmp_vpath2, TRUE);
                vfs_path_free (tmp_vpath1, TRUE);
                g_strlcpy (link_target, s != NULL ? s : p->str, sizeof (link_target));
                g_free (s);
            }

            g_string_free (p, TRUE);
            vfs_path_free (q, TRUE);
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
    if (dst_is_symlink && mc_unlink (dst_vpath) == 0
        && mc_symlink (link_target_vpath, dst_vpath) == 0)
    {
        /* Success */
        return_status = FILE_CONT;
        goto ret;
    }

    if (ctx->skip_all)
        return_status = FILE_SKIPALL;
    else
    {
        return_status = file_error (TRUE, _("Cannot create target symlink \"%s\"\n%s"), dst_path);
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        if (return_status == FILE_RETRY)
            goto retry_dst_symlink;
    }

  ret:
    vfs_path_free (link_target_vpath, TRUE);
    return return_status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * do_compute_dir_size:
 *
 * Computes the number of bytes used by the files in a directory
 */

static FileProgressStatus
do_compute_dir_size (const vfs_path_t * dirname_vpath, dirsize_status_msg_t * dsm,
                     size_t * dir_count, size_t * ret_marked, uintmax_t * ret_total,
                     mc_stat_fn stat_func)
{
    static gint64 timestamp = 0;
    /* update with 25 FPS rate */
    static const gint64 delay = G_USEC_PER_SEC / 25;

    status_msg_t *sm = STATUS_MSG (dsm);
    int res;
    struct stat s;
    DIR *dir;
    struct vfs_dirent *dirent;
    FileProgressStatus ret = FILE_CONT;

    (*dir_count)++;

    dir = mc_opendir (dirname_vpath);
    if (dir == NULL)
        return ret;

    while (ret == FILE_CONT && (dirent = mc_readdir (dir)) != NULL)
    {
        vfs_path_t *tmp_vpath;

        if (DIR_IS_DOT (dirent->d_name) || DIR_IS_DOTDOT (dirent->d_name))
            continue;

        tmp_vpath = vfs_path_append_new (dirname_vpath, dirent->d_name, (char *) NULL);

        res = stat_func (tmp_vpath, &s);
        if (res == 0)
        {
            if (S_ISDIR (s.st_mode))
                ret =
                    do_compute_dir_size (tmp_vpath, dsm, dir_count, ret_marked, ret_total,
                                         stat_func);
            else
            {
                ret = FILE_CONT;

                (*ret_marked)++;
                *ret_total += (uintmax_t) s.st_size;
            }

            if (ret == FILE_CONT && sm->update != NULL && mc_time_elapsed (&timestamp, delay))
            {
                dsm->dirname_vpath = tmp_vpath;
                dsm->dir_count = *dir_count;
                dsm->total_size = *ret_total;
                ret = sm->update (sm);
            }
        }

        vfs_path_free (tmp_vpath, TRUE);
    }

    mc_closedir (dir);
    return ret;
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
panel_compute_totals (const WPanel * panel, dirsize_status_msg_t * sm, size_t * ret_count,
                      uintmax_t * ret_total, gboolean follow_symlinks)
{
    int i;
    size_t dir_count = 0;
    mc_stat_fn stat_func = follow_symlinks ? mc_stat : mc_lstat;

    for (i = 0; i < panel->dir.len; i++)
    {
        const file_entry_t *fe = &panel->dir.list[i];
        const struct stat *s;

        if (fe->f.marked == 0)
            continue;

        s = &fe->st;

        if (S_ISDIR (s->st_mode) || (follow_symlinks && link_isdir (fe) && fe->f.stale_link == 0))
        {
            vfs_path_t *p;
            FileProgressStatus status;

            p = vfs_path_append_new (panel->cwd_vpath, fe->fname->str, (char *) NULL);
            status = do_compute_dir_size (p, sm, &dir_count, ret_count, ret_total, stat_func);
            vfs_path_free (p, TRUE);

            if (status != FILE_CONT)
                return status;
        }
        else
        {
            (*ret_count)++;
            *ret_total += (uintmax_t) s->st_size;
        }
    }

    return FILE_CONT;
}

/* --------------------------------------------------------------------------------------------- */

/** Initialize variables for progress bars */
static FileProgressStatus
panel_operate_init_totals (const WPanel * panel, const vfs_path_t * source,
                           const struct stat *source_stat, file_op_context_t * ctx,
                           gboolean compute_totals, filegui_dialog_type_t dialog_type)
{
    FileProgressStatus status;

#ifdef ENABLE_BACKGROUND
    if (mc_global.we_are_background)
        return FILE_CONT;
#endif

    if (verbose && compute_totals)
    {
        dirsize_status_msg_t dsm;
        gboolean stale_link = FALSE;

        memset (&dsm, 0, sizeof (dsm));
        dsm.allow_skip = TRUE;
        status_msg_init (STATUS_MSG (&dsm), _("Directory scanning"), 0, dirsize_status_init_cb,
                         dirsize_status_update_cb, dirsize_status_deinit_cb);

        ctx->progress_count = 0;
        ctx->progress_bytes = 0;

        if (source == NULL)
            status = panel_compute_totals (panel, &dsm, &ctx->progress_count, &ctx->progress_bytes,
                                           ctx->follow_links);
        else if (S_ISDIR (source_stat->st_mode)
                 || (ctx->follow_links
                     && file_is_symlink_to_dir (source, (struct stat *) source_stat, &stale_link)
                     && !stale_link))
        {
            size_t dir_count = 0;

            status = do_compute_dir_size (source, &dsm, &dir_count, &ctx->progress_count,
                                          &ctx->progress_bytes, ctx->stat_func);
        }
        else
        {
            ctx->progress_count++;
            ctx->progress_bytes += (uintmax_t) source_stat->st_size;
            status = FILE_CONT;
        }

        status_msg_deinit (STATUS_MSG (&dsm));

        ctx->progress_totals_computed = (status == FILE_CONT);

        if (status == FILE_SKIP)
            status = FILE_CONT;
    }
    else
    {
        status = FILE_CONT;
        ctx->progress_count = panel->marked;
        ctx->progress_bytes = panel->total;
        ctx->progress_totals_computed = verbose && dialog_type == FILEGUI_DIALOG_ONE_ITEM;
    }

    /* destroy already created UI for single file rename operation */
    file_op_context_destroy_ui (ctx);

    file_op_context_create_ui (ctx, TRUE, dialog_type);

    return status;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
progress_update_one (file_op_total_context_t * tctx, file_op_context_t * ctx, off_t add)
{
    gint64 tv_current;
    static gint64 tv_start = -1;

    tctx->progress_count++;
    tctx->progress_bytes += (uintmax_t) add;

    tv_current = g_get_monotonic_time ();

    if (tv_start < 0)
        tv_start = tv_current;

    if (tv_current - tv_start > FILEOP_UPDATE_INTERVAL_US)
    {
        if (verbose && ctx->dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
        {
            file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
            file_progress_show_total (tctx, ctx, tctx->progress_bytes, TRUE);
        }

        tv_start = tv_current;
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
    int width_a, width_b, width;

    head_msg = mode == Foreground ? MSG_ERROR : _("Background process error");

    width_a = str_term_width1 (a);
    width_b = str_term_width1 (b);
    width = COLS - 8;

    if (width_a > width)
    {
        if (width_b > width)
        {
            char *s;

            s = g_strndup (str_trunc (a, width), width);
            b = str_trunc (b, width);
            msg = g_strdup_printf (fmt, s, b);
            g_free (s);
        }
        else
        {
            a = str_trunc (a, width);
            msg = g_strdup_printf (fmt, a, b);
        }
    }
    else
    {
        if (width_b > width)
            b = str_trunc (b, width);

        msg = g_strdup_printf (fmt, a, b);
    }

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
        FileProgressStatus (*f) (enum OperationMode, const char *fmt, const char *a, const char *b);
    } pntr;
/* *INDENT-ON* */

    pntr.f = real_warn_same_file;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, NULL, 3, strlen (fmt), fmt, strlen (a), a, strlen (b), b);
#endif
    return real_warn_same_file (Foreground, fmt, a, b);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
check_same_file (const char *a, const struct stat *ast, const char *b, const struct stat *bst,
                 FileProgressStatus * status)
{
    if (ast->st_dev != bst->st_dev || ast->st_ino != bst->st_ino)
        return FALSE;

    if (S_ISDIR (ast->st_mode))
        *status = warn_same_file (_("\"%s\"\nand\n\"%s\"\nare the same directory"), a, b);
    else
        *status = warn_same_file (_("\"%s\"\nand\n\"%s\"\nare the same file"), a, b);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
get_times (const struct stat *sb, mc_timesbuf_t * times)
{
#ifdef HAVE_UTIMENSAT
    (*times)[0] = sb->st_atim;
    (*times)[1] = sb->st_mtim;
#else
    times->actime = sb->st_atime;
    times->modtime = sb->st_mtime;
#endif
}

/* --------------------------------------------------------------------------------------------- */
/* {{{ Query/status report routines */

static FileProgressStatus
real_do_file_error (enum OperationMode mode, gboolean allow_retry, const char *error)
{
    int result;
    const char *msg;

    msg = mode == Foreground ? MSG_ERROR : _("Background process error");

    if (allow_retry)
        result =
            query_dialog (msg, error, D_ERROR, 4, _("&Skip"), _("Ski&p all"), _("&Retry"),
                          _("&Abort"));
    else
        result = query_dialog (msg, error, D_ERROR, 3, _("&Skip"), _("Ski&p all"), _("&Abort"));

    switch (result)
    {
    case 0:
        do_refresh ();
        return FILE_SKIP;

    case 1:
        do_refresh ();
        return FILE_SKIPALL;

    case 2:
        if (allow_retry)
        {
            do_refresh ();
            return FILE_RETRY;
        }
        MC_FALLTHROUGH;

    case 3:
    default:
        return FILE_ABORT;
    }
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
real_query_recursive (file_op_context_t * ctx, enum OperationMode mode, const char *s)
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
            query_dialog (op_names[OP_DELETE], text, D_ERROR, 5, _("&Yes"), _("&No"), _("A&ll"),
                          _("Non&e"), _("&Abort"));
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
do_file_error (gboolean allow_retry, const char *str)
{
/* *INDENT-OFF* */
    union
    {
        void *p;
        FileProgressStatus (*f) (enum OperationMode, gboolean, const char *);
    } pntr;
/* *INDENT-ON* */

    pntr.f = real_do_file_error;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, NULL, 2, sizeof (allow_retry), allow_retry, strlen (str), str);
    else
        return real_do_file_error (Foreground, allow_retry, str);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_recursive (file_op_context_t * ctx, const char *s)
{
/* *INDENT-OFF* */
    union
    {
        void *p;
        FileProgressStatus (*f) (file_op_context_t *, enum OperationMode, const char *);
    } pntr;
/* *INDENT-ON* */

    pntr.f = real_query_recursive;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, ctx, 1, strlen (s), s);
    else
        return real_query_recursive (ctx, Foreground, s);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_replace (file_op_context_t * ctx, const char *src, struct stat *src_stat, const char *dst,
               struct stat *dst_stat)
{
/* *INDENT-OFF* */
    union
    {
        void *p;
        FileProgressStatus (*f) (file_op_context_t *, enum OperationMode, const char *,
                                 struct stat *, const char *, struct stat *);
    } pntr;
/* *INDENT-ON* */

    pntr.f = file_progress_real_query_replace;

    if (mc_global.we_are_background)
        return parent_call (pntr.p, ctx, 4, strlen (src), src, sizeof (struct stat), src_stat,
                            strlen (dst), dst, sizeof (struct stat), dst_stat);
    else
        return file_progress_real_query_replace (ctx, Foreground, src, src_stat, dst, dst_stat);
}

#else
/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
do_file_error (gboolean allow_retry, const char *str)
{
    return real_do_file_error (Foreground, allow_retry, str);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_recursive (file_op_context_t * ctx, const char *s)
{
    return real_query_recursive (ctx, Foreground, s);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
query_replace (file_op_context_t * ctx, const char *src, struct stat *src_stat, const char *dst,
               struct stat *dst_stat)
{
    return file_progress_real_query_replace (ctx, Foreground, src, src_stat, dst, dst_stat);
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

    return do_file_error (TRUE, buf);
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */

static void
copy_file_file_display_progress (file_op_total_context_t * tctx, file_op_context_t * ctx,
                                 gint64 tv_current, gint64 tv_transfer_start, off_t file_size,
                                 off_t file_part)
{
    gint64 dt;

    /* Update rotating dash after some time */
    rotate_dash (TRUE);

    /* Compute ETA */
    dt = (tv_current - tv_transfer_start) / G_USEC_PER_SEC;

    if (file_part == 0)
        ctx->eta_secs = 0.0;
    else
        ctx->eta_secs = ((dt / (double) file_part) * file_size) - dt;

    /* Compute BPS rate */
    ctx->bps_time = MAX (1, dt);
    ctx->bps = file_part / ctx->bps_time;

    /* Compute total ETA and BPS */
    if (ctx->progress_bytes != 0)
    {
        uintmax_t remain_bytes;

        remain_bytes = ctx->progress_bytes - tctx->copied_bytes;
#if 1
        {
            gint64 total_secs;

            total_secs = (tv_current - tctx->transfer_start) / G_USEC_PER_SEC;
            total_secs = MAX (1, total_secs);

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

static gboolean
try_remove_file (file_op_context_t * ctx, const vfs_path_t * vpath, FileProgressStatus * status)
{
    while (mc_unlink (vpath) != 0 && !ctx->skip_all)
    {
        *status = file_error (TRUE, _("Cannot remove file \"%s\"\n%s"), vfs_path_as_str (vpath));
        if (*status == FILE_RETRY)
            continue;
        if (*status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Move routines */

/**
 * Move single file or one of many files from one location to another.
 *
 * @panel pointer to panel in case of single file, NULL otherwise
 * @tctx file operation total context object
 * @ctx file operation context object
 * @s source file name
 * @d destination file name
 *
 * @return operation result
 */
static FileProgressStatus
move_file_file (const WPanel * panel, file_op_total_context_t * tctx, file_op_context_t * ctx,
                const char *s, const char *d)
{
    struct stat src_stat, dst_stat;
    FileProgressStatus return_status = FILE_CONT;
    gboolean copy_done = FALSE;
    gboolean old_ask_overwrite;
    vfs_path_t *src_vpath, *dst_vpath;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    file_progress_show_source (ctx, src_vpath);
    file_progress_show_target (ctx, dst_vpath);

    /* FIXME: do we really need to check buttons in case of single file? */
    if (check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret;
    }

    mc_refresh ();

    while (mc_lstat (src_vpath, &src_stat) != 0)
    {
        /* Source doesn't exist */
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot stat file \"%s\"\n%s"), s);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }

        if (return_status != FILE_RETRY)
            goto ret;
    }

    if (mc_lstat (dst_vpath, &dst_stat) == 0)
    {
        if (check_same_file (s, &src_stat, d, &dst_stat, &return_status))
            goto ret;

        if (S_ISDIR (dst_stat.st_mode))
        {
            message (D_ERROR, MSG_ERROR, _("Cannot overwrite directory \"%s\""), d);
            do_refresh ();
            return_status = FILE_SKIP;
            goto ret;
        }

        if (confirm_overwrite)
        {
            return_status = query_replace (ctx, s, &src_stat, d, &dst_stat);
            if (return_status != FILE_CONT)
                goto ret;
        }
        /* Ok to overwrite */
    }

    if (!ctx->do_append)
    {
        if (S_ISLNK (src_stat.st_mode) && ctx->stable_symlinks)
        {
            return_status = make_symlink (ctx, src_vpath, dst_vpath);
            if (return_status == FILE_CONT)
            {
                if (ctx->preserve)
                {
                    mc_timesbuf_t times;

                    get_times (&src_stat, &times);
                    mc_utime (dst_vpath, &times);
                }
                goto retry_src_remove;
            }
            goto ret;
        }

        if (mc_rename (src_vpath, dst_vpath) == 0)
        {
            /* FIXME: do we really need to update progress in case of single file? */
            return_status = progress_update_one (tctx, ctx, src_stat.st_size);
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

    /* Failed rename -> copy the file instead */
    if (panel != NULL)
    {
        /* In case of single file, calculate totals. In case of many files,
           totals are calculated already. */
        return_status =
            panel_operate_init_totals (panel, src_vpath, &src_stat, ctx, TRUE,
                                       FILEGUI_DIALOG_ONE_ITEM);
        if (return_status != FILE_CONT)
            goto ret;
    }

    old_ask_overwrite = tctx->ask_overwrite;
    tctx->ask_overwrite = FALSE;
    return_status = copy_file_file (tctx, ctx, s, d);
    tctx->ask_overwrite = old_ask_overwrite;
    if (return_status != FILE_CONT)
        goto ret;

    copy_done = TRUE;

    /* FIXME: there is no need to update progress and check buttons
       at the finish of single file operation. */
    if (panel == NULL)
    {
        file_progress_show_source (ctx, NULL);
        file_progress_show (ctx, 0, 0, "", FALSE);

        return_status = check_progress_buttons (ctx);
        if (return_status != FILE_CONT)
            goto ret;
    }

    mc_refresh ();

  retry_src_remove:
    if (!try_remove_file (ctx, src_vpath, &return_status) && panel == NULL)
        goto ret;

    if (!copy_done)
        return_status = progress_update_one (tctx, ctx, src_stat.st_size);

  ret:
    vfs_path_free (src_vpath, TRUE);
    vfs_path_free (dst_vpath, TRUE);

    return return_status;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Erase routines */
/** Don't update progress status if progress_count==NULL */

static FileProgressStatus
erase_file (file_op_total_context_t * tctx, file_op_context_t * ctx, const vfs_path_t * vpath)
{
    struct stat buf;
    FileProgressStatus return_status;

    /* check buttons if deleting info was changed */
    if (file_progress_show_deleting (ctx, vfs_path_as_str (vpath), &tctx->progress_count))
    {
        file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
        if (check_progress_buttons (ctx) == FILE_ABORT)
            return FILE_ABORT;

        mc_refresh ();
    }

    if (tctx->progress_count != 0 && mc_lstat (vpath, &buf) != 0)
    {
        /* ignore, most likely the mc_unlink fails, too */
        buf.st_size = 0;
    }

    if (!try_remove_file (ctx, vpath, &return_status) && return_status == FILE_ABORT)
        return FILE_ABORT;

    if (tctx->progress_count == 0)
        return FILE_CONT;

    return check_progress_buttons (ctx);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
try_erase_dir (file_op_context_t * ctx, const char *dir)
{
    FileProgressStatus return_status = FILE_CONT;

    while (my_rmdir (dir) != 0 && !ctx->skip_all)
    {
        return_status = file_error (TRUE, _("Cannot remove directory \"%s\"\n%s"), dir);
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        if (return_status != FILE_RETRY)
            break;
    }

    return return_status;
}

/* --------------------------------------------------------------------------------------------- */

/**
  Recursive remove of files
  abort->cancel stack
  skip ->warn every level, gets default
  skipall->remove as much as possible
*/
static FileProgressStatus
recursive_erase (file_op_total_context_t * tctx, file_op_context_t * ctx, const vfs_path_t * vpath)
{
    struct vfs_dirent *next;
    DIR *reading;
    const char *s;
    FileProgressStatus return_status = FILE_CONT;

    reading = mc_opendir (vpath);
    if (reading == NULL)
        return FILE_RETRY;

    while ((next = mc_readdir (reading)) && return_status != FILE_ABORT)
    {
        vfs_path_t *tmp_vpath;
        struct stat buf;

        if (DIR_IS_DOT (next->d_name) || DIR_IS_DOTDOT (next->d_name))
            continue;

        tmp_vpath = vfs_path_append_new (vpath, next->d_name, (char *) NULL);
        if (mc_lstat (tmp_vpath, &buf) != 0)
        {
            mc_closedir (reading);
            vfs_path_free (tmp_vpath, TRUE);
            return FILE_RETRY;
        }
        if (S_ISDIR (buf.st_mode))
            return_status = recursive_erase (tctx, ctx, tmp_vpath);
        else
            return_status = erase_file (tctx, ctx, tmp_vpath);
        vfs_path_free (tmp_vpath, TRUE);
    }
    mc_closedir (reading);

    if (return_status == FILE_ABORT)
        return FILE_ABORT;

    s = vfs_path_as_str (vpath);

    file_progress_show_deleting (ctx, s, NULL);
    file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
    if (check_progress_buttons (ctx) == FILE_ABORT)
        return FILE_ABORT;

    mc_refresh ();

    return try_erase_dir (ctx, s);
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Check if directory is empty or not.
  *
  * @param vpath directory handler
  *
  * @returns -1 on error,
  *          1 if there are no entries besides "." and ".." in the directory path points to,
  *          0 else.
  *
  * ATTENTION! Be careful when modifying this function (like commit 25e419ba0886f)!
  * Some implementations of readdir() in MC VFS (for example, vfs_s_readdir(), which is used
  * in SHELL) don't return "." and ".." entries.
  */
static int
check_dir_is_empty (const vfs_path_t * vpath)
{
    DIR *dir;
    struct vfs_dirent *d;
    int i = 1;

    dir = mc_opendir (vpath);
    if (dir == NULL)
        return -1;

    for (d = mc_readdir (dir); d != NULL; d = mc_readdir (dir))
        if (!DIR_IS_DOT (d->d_name) && !DIR_IS_DOTDOT (d->d_name))
        {
            i = 0;
            break;
        }

    mc_closedir (dir);
    return i;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
erase_dir_iff_empty (file_op_context_t * ctx, const vfs_path_t * vpath, size_t count)
{
    const char *s;

    s = vfs_path_as_str (vpath);

    file_progress_show_deleting (ctx, s, NULL);
    file_progress_show_count (ctx, count, ctx->progress_count);
    if (check_progress_buttons (ctx) == FILE_ABORT)
        return FILE_ABORT;

    mc_refresh ();

    if (check_dir_is_empty (vpath) != 1)
        return FILE_CONT;

    /* not empty or error */
    return try_erase_dir (ctx, s);
}


/* --------------------------------------------------------------------------------------------- */

static void
erase_dir_after_copy (file_op_total_context_t * tctx, file_op_context_t * ctx,
                      const vfs_path_t * vpath, FileProgressStatus * status)
{
    if (ctx->erase_at_end && erase_list != NULL)
    {
        /* Reset progress count before delete to avoid counting files twice */
        tctx->progress_count = tctx->prev_progress_count;

        while (!g_queue_is_empty (erase_list) && *status != FILE_ABORT)
        {
            struct link *lp;

            lp = (struct link *) g_queue_pop_head (erase_list);

            if (S_ISDIR (lp->st_mode))
                *status = erase_dir_iff_empty (ctx, lp->src_vpath, tctx->progress_count);
            else
                *status = erase_file (tctx, ctx, lp->src_vpath);

            free_link (lp);
        }

        /* Save progress counter before move next directory */
        tctx->prev_progress_count = tctx->progress_count;
    }

    erase_dir_iff_empty (ctx, vpath, tctx->progress_count);
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */

/**
 * Move single directory or one of many directories from one location to another.
 *
 * @panel pointer to panel in case of single directory, NULL otherwise
 * @tctx file operation total context object
 * @ctx file operation context object
 * @s source directory name
 * @d destination directory name
 *
 * @return operation result
 */
static FileProgressStatus
do_move_dir_dir (const WPanel * panel, file_op_total_context_t * tctx, file_op_context_t * ctx,
                 const char *s, const char *d)
{
    struct stat src_stat, dst_stat;
    FileProgressStatus return_status = FILE_CONT;
    gboolean move_over = FALSE;
    gboolean dstat_ok;
    vfs_path_t *src_vpath, *dst_vpath;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    file_progress_show_source (ctx, src_vpath);
    file_progress_show_target (ctx, dst_vpath);

    /* FIXME: do we really need to check buttons in case of single directory? */
    if (panel != NULL && check_progress_buttons (ctx) == FILE_ABORT)
    {
        return_status = FILE_ABORT;
        goto ret_fast;
    }

    mc_refresh ();

    mc_stat (src_vpath, &src_stat);

    dstat_ok = (mc_stat (dst_vpath, &dst_stat) == 0);

    if (dstat_ok && check_same_file (s, &src_stat, d, &dst_stat, &return_status))
        goto ret_fast;

    if (!dstat_ok)
        ;                       /* destination doesn't exist */
    else if (!ctx->dive_into_subdirs)
        move_over = TRUE;
    else
    {
        vfs_path_t *tmp;

        tmp = dst_vpath;
        dst_vpath = vfs_path_append_new (dst_vpath, x_basename (s), (char *) NULL);
        vfs_path_free (tmp, TRUE);
    }

    d = vfs_path_as_str (dst_vpath);

    /* Check if the user inputted an existing dir */
  retry_dst_stat:
    if (mc_stat (dst_vpath, &dst_stat) == 0)
    {
        if (move_over)
        {
            if (panel != NULL)
            {
                /* In case of single directory, calculate totals. In case of many directories,
                   totals are calculated already. */
                return_status =
                    panel_operate_init_totals (panel, src_vpath, &src_stat, ctx, TRUE,
                                               FILEGUI_DIALOG_MULTI_ITEM);
                if (return_status != FILE_CONT)
                    goto ret;
            }

            return_status = copy_dir_dir (tctx, ctx, s, d, FALSE, TRUE, TRUE, NULL);

            if (return_status != FILE_CONT)
                goto ret;
            goto oktoret;
        }
        else if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            if (S_ISDIR (dst_stat.st_mode))
                return_status = file_error (TRUE, _("Cannot overwrite directory \"%s\"\n%s"), d);
            else
                return_status = file_error (TRUE, _("Cannot overwrite file \"%s\"\n%s"), d);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            if (return_status == FILE_RETRY)
                goto retry_dst_stat;
        }

        goto ret_fast;
    }

  retry_rename:
    if (mc_rename (src_vpath, dst_vpath) == 0)
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
    if (panel != NULL)
    {
        /* In case of single directory, calculate totals. In case of many directories,
           totals are calculated already. */
        return_status =
            panel_operate_init_totals (panel, src_vpath, &src_stat, ctx, TRUE,
                                       FILEGUI_DIALOG_MULTI_ITEM);
        if (return_status != FILE_CONT)
            goto ret;
    }

    return_status = copy_dir_dir (tctx, ctx, s, d, FALSE, FALSE, TRUE, NULL);

    if (return_status != FILE_CONT)
        goto ret;

  oktoret:
    /* FIXME: there is no need to update progress and check buttons
       at the finish of single directory operation. */
    if (panel == NULL)
    {
        file_progress_show_source (ctx, NULL);
        file_progress_show_target (ctx, NULL);
        file_progress_show (ctx, 0, 0, "", FALSE);

        return_status = check_progress_buttons (ctx);
        if (return_status != FILE_CONT)
            goto ret;
    }

    mc_refresh ();

    erase_dir_after_copy (tctx, ctx, src_vpath, &return_status);

  ret:
    erase_list = free_erase_list (erase_list);
  ret_fast:
    vfs_path_free (src_vpath, TRUE);
    vfs_path_free (dst_vpath, TRUE);
    return return_status;
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Panel operate routines */

/**
 * Return currently selected entry name or the name of the first marked
 * entry if there is one.
 */

static const char *
panel_get_file (const WPanel * panel)
{
    if (get_current_type () == view_tree)
    {
        WTree *tree;
        const vfs_path_t *selected_name;

        tree = (WTree *) get_panel_widget (get_current_index ());
        selected_name = tree_selected_name (tree);
        return vfs_path_as_str (selected_name);
    }

    if (panel->marked != 0)
    {
        int i;

        for (i = 0; i < panel->dir.len; i++)
            if (panel->dir.list[i].f.marked != 0)
                return panel->dir.list[i].fname->str;
    }

    return panel_current_entry (panel)->fname->str;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
check_single_entry (const WPanel * panel, gboolean force_single, struct stat *src_stat)
{
    const char *source;
    gboolean ok;

    if (force_single)
        source = panel_current_entry (panel)->fname->str;
    else
        source = panel_get_file (panel);

    ok = !DIR_IS_DOTDOT (source);

    if (!ok)
        message (D_ERROR, MSG_ERROR, _("Cannot operate on \"..\"!"));
    else
    {
        vfs_path_t *source_vpath;

        source_vpath = vfs_path_from_str (source);

        /* Update stat to get actual info */
        ok = mc_lstat (source_vpath, src_stat) == 0;
        if (!ok)
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
        }

        vfs_path_free (source_vpath, TRUE);
    }

    return ok ? source : NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Generate user prompt for panel operation.
 * src_stat must be not NULL for single source, and NULL for multiple sources
 */

static char *
panel_operate_generate_prompt (const WPanel * panel, FileOperation operation,
                               const struct stat *src_stat)
{
    char *sp;
    char *format_string;
    const char *cp;

    static gboolean i18n_flag = FALSE;
    if (!i18n_flag)
    {
        size_t i;

        for (i = G_N_ELEMENTS (op_names1); i-- != 0;)
            op_names1[i] = Q_ (op_names1[i]);

#ifdef ENABLE_NLS
        for (i = G_N_ELEMENTS (prompt_parts); i-- != 0;)
            prompt_parts[i] = _(prompt_parts[i]);

        one_format = _(one_format);
        many_format = _(many_format);
#endif /* ENABLE_NLS */
        i18n_flag = TRUE;
    }

    /* Possible prompts:
     *   OP_COPY:
     *       "Copy file \"%s\" with source mask:"
     *       "Copy %d files with source mask:"
     *       "Copy directory \"%s\" with source mask:"
     *       "Copy %d directories with source mask:"
     *       "Copy %d files/directories with source mask:"
     *   OP_MOVE:
     *       "Move file \"%s\" with source mask:"
     *       "Move %d files with source mask:"
     *       "Move directory \"%s\" with source mask:"
     *       "Move %d directories with source mask:"
     *       "Move %d files/directories with source mask:"
     *   OP_DELETE:
     *       "Delete file \"%s\"?"
     *       "Delete %d files?"
     *       "Delete directory \"%s\"?"
     *       "Delete %d directories?"
     *       "Delete %d files/directories?"
     */

    cp = (src_stat != NULL ? one_format : many_format);

    /* 1. Substitute %o */
    format_string = str_replace_all (cp, "%o", op_names1[(int) operation]);

    /* 2. Substitute %n */
    cp = operation == OP_DELETE ? "\n" : " ";
    sp = format_string;
    format_string = str_replace_all (sp, "%n", cp);
    g_free (sp);

    /* 3. Substitute %f */
    if (src_stat != NULL)
        cp = S_ISDIR (src_stat->st_mode) ? prompt_parts[2] : prompt_parts[0];
    else if (panel->marked == panel->dirs_marked)
        cp = prompt_parts[3];
    else
        cp = panel->dirs_marked != 0 ? prompt_parts[4] : prompt_parts[1];

    sp = format_string;
    format_string = str_replace_all (sp, "%f", cp);
    g_free (sp);

    /* 4. Substitute %m */
    cp = operation == OP_DELETE ? "?" : prompt_parts[5];
    sp = format_string;
    format_string = str_replace_all (sp, "%m", cp);
    g_free (sp);

    return format_string;
}

/* --------------------------------------------------------------------------------------------- */

static char *
do_confirm_copy_move (const WPanel * panel, gboolean force_single, const char *source,
                      struct stat *src_stat, file_op_context_t * ctx, gboolean * do_bg)
{
    const char *tmp_dest_dir;
    char *dest_dir;
    char *format;
    char *ret;

    /* Forced single operations default to the original name */
    if (force_single)
        tmp_dest_dir = source;
    else if (get_other_type () == view_listing)
        tmp_dest_dir = vfs_path_as_str (other_panel->cwd_vpath);
    else
        tmp_dest_dir = vfs_path_as_str (panel->cwd_vpath);

    /*
     * Add trailing backslash only when do non-local ops.
     * It saves user from occasional file renames (when destination
     * dir is deleted)
     */
    if (!force_single && tmp_dest_dir != NULL && tmp_dest_dir[0] != '\0'
        && !IS_PATH_SEP (tmp_dest_dir[strlen (tmp_dest_dir) - 1]))
    {
        /* add trailing separator */
        dest_dir = g_strconcat (tmp_dest_dir, PATH_SEP_STR, (char *) NULL);
    }
    else
    {
        /* just copy */
        dest_dir = g_strdup (tmp_dest_dir);
    }

    if (dest_dir == NULL)
        return NULL;

    if (source == NULL)
        src_stat = NULL;

    /* Generate confirmation prompt */
    format = panel_operate_generate_prompt (panel, ctx->operation, src_stat);

    ret = file_mask_dialog (ctx, source != NULL, format,
                            source != NULL ? source : (const void *) &panel->marked, dest_dir,
                            do_bg);

    g_free (format);
    g_free (dest_dir);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
do_confirm_erase (const WPanel * panel, const char *source, struct stat *src_stat)
{
    int i;
    char *format;
    char fmd_buf[BUF_MEDIUM];

    if (source == NULL)
        src_stat = NULL;

    /* Generate confirmation prompt */
    format = panel_operate_generate_prompt (panel, OP_DELETE, src_stat);

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

    i = query_dialog (op_names[OP_DELETE], fmd_buf, D_ERROR, 2, _("&Yes"), _("&No"));

    return (i == 0);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
operate_single_file (const WPanel * panel, file_op_total_context_t * tctx, file_op_context_t * ctx,
                     const char *src, struct stat *src_stat, const char *dest,
                     filegui_dialog_type_t dialog_type)
{
    FileProgressStatus value;
    vfs_path_t *src_vpath;
    gboolean is_file;

    if (g_path_is_absolute (src))
        src_vpath = vfs_path_from_str (src);
    else
        src_vpath = vfs_path_append_new (panel->cwd_vpath, src, (char *) NULL);

    is_file = !S_ISDIR (src_stat->st_mode);
    /* Is link to directory? */
    if (is_file)
    {
        gboolean is_link;

        is_link = file_is_symlink_to_dir (src_vpath, src_stat, NULL);
        is_file = !(is_link && ctx->follow_links);
    }

    if (ctx->operation == OP_DELETE)
    {
        value = panel_operate_init_totals (panel, src_vpath, src_stat, ctx, !is_file, dialog_type);
        if (value == FILE_CONT)
        {
            if (is_file)
                value = erase_file (tctx, ctx, src_vpath);
            else
                value = erase_dir (tctx, ctx, src_vpath);
        }
    }
    else
    {
        char *temp;

        src = vfs_path_as_str (src_vpath);

        temp = build_dest (ctx, src, dest, &value);
        if (temp != NULL)
        {
            dest = temp;

            switch (ctx->operation)
            {
            case OP_COPY:
                /* we use file_mask_op_follow_links only with OP_COPY */
                ctx->stat_func (src_vpath, src_stat);

                value =
                    panel_operate_init_totals (panel, src_vpath, src_stat, ctx, !is_file,
                                               dialog_type);
                if (value == FILE_CONT)
                {
                    is_file = !S_ISDIR (src_stat->st_mode);
                    /* Is link to directory? */
                    if (is_file)
                    {
                        gboolean is_link;

                        is_link = file_is_symlink_to_dir (src_vpath, src_stat, NULL);
                        is_file = !(is_link && ctx->follow_links);
                    }

                    if (is_file)
                        value = copy_file_file (tctx, ctx, src, dest);
                    else
                        value = copy_dir_dir (tctx, ctx, src, dest, TRUE, FALSE, FALSE, NULL);
                }
                break;

            case OP_MOVE:
#ifdef ENABLE_BACKGROUND
                if (!mc_global.we_are_background)
#endif
                    /* create UI to show confirmation dialog */
                    file_op_context_create_ui (ctx, TRUE, FILEGUI_DIALOG_ONE_ITEM);

                if (is_file)
                    value = move_file_file (panel, tctx, ctx, src, dest);
                else
                    value = do_move_dir_dir (panel, tctx, ctx, src, dest);
                break;

            default:
                /* Unknown file operation */
                abort ();
            }

            g_free (temp);
        }
    }

    vfs_path_free (src_vpath, TRUE);

    return value;
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
operate_one_file (const WPanel * panel, file_op_total_context_t * tctx, file_op_context_t * ctx,
                  const char *src, struct stat *src_stat, const char *dest)
{
    FileProgressStatus value = FILE_CONT;
    vfs_path_t *src_vpath;
    gboolean is_file;

    if (g_path_is_absolute (src))
        src_vpath = vfs_path_from_str (src);
    else
        src_vpath = vfs_path_append_new (panel->cwd_vpath, src, (char *) NULL);

    is_file = !S_ISDIR (src_stat->st_mode);

    if (ctx->operation == OP_DELETE)
    {
        if (is_file)
            value = erase_file (tctx, ctx, src_vpath);
        else
            value = erase_dir (tctx, ctx, src_vpath);
    }
    else
    {
        char *temp;

        src = vfs_path_as_str (src_vpath);

        temp = build_dest (ctx, src, dest, &value);
        if (temp != NULL)
        {
            dest = temp;

            switch (ctx->operation)
            {
            case OP_COPY:
                /* we use file_mask_op_follow_links only with OP_COPY */
                ctx->stat_func (src_vpath, src_stat);
                is_file = !S_ISDIR (src_stat->st_mode);

                if (is_file)
                    value = copy_file_file (tctx, ctx, src, dest);
                else
                    value = copy_dir_dir (tctx, ctx, src, dest, TRUE, FALSE, FALSE, NULL);
                dest_dirs = free_linklist (dest_dirs);
                break;

            case OP_MOVE:
                if (is_file)
                    value = move_file_file (NULL, tctx, ctx, src, dest);
                else
                    value = do_move_dir_dir (NULL, tctx, ctx, src, dest);
                break;

            default:
                /* Unknown file operation */
                abort ();
            }

            g_free (temp);
        }
    }

    vfs_path_free (src_vpath, TRUE);

    return value;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static int
end_bg_process (file_op_context_t * ctx, enum OperationMode mode)
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

/* Is file symlink to directory or not.
 *
 * @param path file or directory
 * @param st result of mc_lstat(vpath). If NULL, mc_lstat(vpath) is performed here
 * @param stale_link TRUE if file is stale link to directory
 *
 * @return TRUE if file symlink to directory, ELSE otherwise.
 */
gboolean
file_is_symlink_to_dir (const vfs_path_t * vpath, struct stat * st, gboolean * stale_link)
{
    struct stat st2;
    gboolean stale = FALSE;
    gboolean res = FALSE;

    if (st == NULL)
    {
        st = &st2;

        if (mc_lstat (vpath, st) != 0)
            goto ret;
    }

    if (S_ISLNK (st->st_mode))
    {
        struct stat st3;

        stale = (mc_stat (vpath, &st3) != 0);

        if (!stale)
            res = (S_ISDIR (st3.st_mode) != 0);
    }

  ret:
    if (stale_link != NULL)
        *stale_link = stale;

    return res;
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
copy_file_file (file_op_total_context_t * tctx, file_op_context_t * ctx,
                const char *src_path, const char *dst_path)
{
    uid_t src_uid = (uid_t) (-1);
    gid_t src_gid = (gid_t) (-1);

    int src_desc, dest_desc = -1;
    mode_t src_mode = 0;        /* The mode of the source file */
    struct stat src_stat, dst_stat;
    mc_timesbuf_t times;
    gboolean dst_exists = FALSE, appending = FALSE;
    off_t file_size = -1;
    FileProgressStatus return_status, temp_status;
    gint64 tv_transfer_start;
    dest_status_t dst_status = DEST_NONE;
    int open_flags;
    vfs_path_t *src_vpath = NULL, *dst_vpath = NULL;
    char *buf = NULL;

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

    while (mc_stat (dst_vpath, &dst_stat) == 0)
    {
        if (S_ISDIR (dst_stat.st_mode))
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (TRUE, _("Cannot overwrite directory \"%s\"\n%s"), dst_path);
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

    while ((*ctx->stat_func) (src_vpath, &src_stat) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot stat source file \"%s\"\n%s"), src_path);
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }

        if (return_status != FILE_RETRY)
            goto ret_fast;
    }

    if (dst_exists)
    {
        /* Destination already exists */
        if (check_same_file (src_path, &src_stat, dst_path, &dst_stat, &return_status))
            goto ret_fast;

        /* Should we replace destination? */
        if (tctx->ask_overwrite)
        {
            ctx->do_reget = 0;
            return_status = query_replace (ctx, src_path, &src_stat, dst_path, &dst_stat);
            if (return_status != FILE_CONT)
                goto ret_fast;
        }
    }

    get_times (&src_stat, &times);

    if (!ctx->do_append)
    {
        /* Check the hardlinks */
        if (!ctx->follow_links)
        {
            switch (check_hardlinks (src_vpath, &src_stat, dst_vpath, &ctx->skip_all))
            {
            case HARDLINK_OK:
                /* We have made a hardlink - no more processing is necessary */
                return_status = FILE_CONT;
                goto ret_fast;

            case HARDLINK_ABORT:
                return_status = FILE_ABORT;
                goto ret_fast;

            default:
                break;
            }
        }

        if (S_ISLNK (src_stat.st_mode))
        {
            return_status = make_symlink (ctx, src_vpath, dst_vpath);
            if (return_status == FILE_CONT && ctx->preserve)
                mc_utime (dst_vpath, &times);
            goto ret_fast;
        }

        if (S_ISCHR (src_stat.st_mode) || S_ISBLK (src_stat.st_mode) || S_ISFIFO (src_stat.st_mode)
            || S_ISNAM (src_stat.st_mode) || S_ISSOCK (src_stat.st_mode))
        {
            dev_t rdev = 0;

#ifdef HAVE_STRUCT_STAT_ST_RDEV
            rdev = src_stat.st_rdev;
#endif

            while (mc_mknod (dst_vpath, src_stat.st_mode & ctx->umask_kill, rdev) < 0
                   && !ctx->skip_all)
            {
                return_status =
                    file_error (TRUE, _("Cannot create special file \"%s\"\n%s"), dst_path);
                if (return_status == FILE_RETRY)
                    continue;
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                goto ret_fast;
            }
            /* Success */

            while (ctx->preserve_uidgid
                   && mc_chown (dst_vpath, src_stat.st_uid, src_stat.st_gid) != 0 && !ctx->skip_all)
            {
                temp_status = file_error (TRUE, _("Cannot chown target file \"%s\"\n%s"), dst_path);
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

            while (ctx->preserve && mc_chmod (dst_vpath, src_stat.st_mode & ctx->umask_kill) != 0
                   && !ctx->skip_all)
            {
                temp_status = file_error (TRUE, _("Cannot chmod target file \"%s\"\n%s"), dst_path);
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
            mc_utime (dst_vpath, &times);
            goto ret_fast;
        }
    }

    tv_transfer_start = g_get_monotonic_time ();

    while ((src_desc = mc_open (src_vpath, O_RDONLY | O_LINEAR)) < 0 && !ctx->skip_all)
    {
        return_status = file_error (TRUE, _("Cannot open source file \"%s\"\n%s"), src_path);
        if (return_status == FILE_RETRY)
            continue;
        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        if (return_status == FILE_SKIP)
            break;
        ctx->do_append = FALSE;
        goto ret_fast;
    }

    if (ctx->do_reget != 0 && mc_lseek (src_desc, ctx->do_reget, SEEK_SET) != ctx->do_reget)
    {
        message (D_ERROR, _("Warning"), _("Reget failed, about to overwrite file"));
        ctx->do_reget = 0;
        ctx->do_append = FALSE;
    }

    while (mc_fstat (src_desc, &src_stat) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot fstat source file \"%s\"\n%s"), src_path);
            if (return_status == FILE_RETRY)
                continue;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
            ctx->do_append = FALSE;
        }
        goto ret;
    }

    src_mode = src_stat.st_mode;
    src_uid = src_stat.st_uid;
    src_gid = src_stat.st_gid;
    file_size = src_stat.st_size;

    open_flags = O_WRONLY;
    if (!dst_exists)
        open_flags |= O_CREAT | O_EXCL;
    else if (ctx->do_append)
        open_flags |= O_APPEND;
    else
        open_flags |= O_CREAT | O_TRUNC;

    while ((dest_desc = mc_open (dst_vpath, open_flags, src_mode)) < 0)
    {
        if (errno != EEXIST)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (TRUE, _("Cannot create target file \"%s\"\n%s"), dst_path);
                if (return_status == FILE_RETRY)
                    continue;
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                ctx->do_append = FALSE;
            }
        }
        goto ret;
    }

    /* file opened, but not fully copied */
    dst_status = DEST_SHORT_QUERY;

    appending = ctx->do_append;
    ctx->do_append = FALSE;

    /* Try clone the file first. */
    if (vfs_clone_file (dest_desc, src_desc) == 0)
    {
        dst_status = DEST_FULL;
        return_status = FILE_CONT;
        goto ret;
    }

    /* Find out the optimal buffer size.  */
    while (mc_fstat (dest_desc, &dst_stat) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot fstat target file \"%s\"\n%s"), dst_path);
            if (return_status == FILE_RETRY)
                continue;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret;
    }

    /* try preallocate space; if fail, try copy anyway */
    while (mc_global.vfs.preallocate_space &&
           vfs_preallocate (dest_desc, file_size, appending ? dst_stat.st_size : 0) != 0)
    {
        if (ctx->skip_all)
        {
            /* cannot allocate, start the file copying anyway */
            return_status = FILE_CONT;
            break;
        }

        return_status =
            file_error (TRUE, _("Cannot preallocate space for target file \"%s\"\n%s"), dst_path);

        if (return_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;

        if (ctx->skip_all || return_status == FILE_SKIP)
        {
            /* skip the space allocation error, start file copying */
            return_status = FILE_CONT;
            break;
        }

        if (return_status == FILE_ABORT)
        {
            mc_close (dest_desc);
            dest_desc = -1;
            mc_unlink (dst_vpath);
            dst_status = DEST_NONE;
            goto ret;
        }

        /* return_status == FILE_RETRY -- try allocate space again */
    }

    ctx->eta_secs = 0.0;
    ctx->bps = 0;

    if (tctx->bps == 0 || (file_size / tctx->bps) > FILEOP_UPDATE_INTERVAL)
        file_progress_show (ctx, 0, file_size, "", TRUE);
    else
        file_progress_show (ctx, 1, 1, "", TRUE);
    return_status = check_progress_buttons (ctx);
    mc_refresh ();

    if (return_status == FILE_CONT)
    {
        size_t bufsize;
        off_t file_part = 0;
        gint64 tv_current, tv_last_update;
        gint64 tv_last_input = 0;
        gint64 usecs, update_usecs;
        const char *stalled_msg = "";
        gboolean is_first_time = TRUE;

        tv_last_update = tv_transfer_start;

        bufsize = io_blksize (dst_stat);
        buf = g_malloc (bufsize);

        while (TRUE)
        {
            ssize_t n_read = -1, n_written;
            gboolean force_update;

            /* src_read */
            if (mc_ctl (src_desc, VFS_CTL_IS_NOTREADY, 0) == 0)
                while ((n_read = mc_read (src_desc, buf, bufsize)) < 0 && !ctx->skip_all)
                {
                    return_status =
                        file_error (TRUE, _("Cannot read source file \"%s\"\n%s"), src_path);
                    if (return_status == FILE_RETRY)
                        continue;
                    if (return_status == FILE_SKIPALL)
                        ctx->skip_all = TRUE;
                    goto ret;
                }

            if (n_read == 0)
                break;

            tv_current = g_get_monotonic_time ();

            if (n_read > 0)
            {
                char *t = buf;

                file_part += n_read;

                tv_last_input = tv_current;

                /* dst_write */
                while ((n_written = mc_write (dest_desc, t, (size_t) n_read)) < n_read)
                {
                    gboolean write_errno_nospace;

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
                            file_error (TRUE, _("Cannot write target file \"%s\"\n%s"), dst_path);

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
                }
            }

            tctx->copied_bytes = tctx->progress_bytes + file_part + ctx->do_reget;

            usecs = tv_current - tv_last_update;
            update_usecs = tv_current - tv_last_input;

            if (is_first_time || usecs > FILEOP_UPDATE_INTERVAL_US)
            {
                copy_file_file_display_progress (tctx, ctx, tv_current, tv_transfer_start,
                                                 file_size, file_part);
                tv_last_update = tv_current;
            }

            is_first_time = FALSE;

            if (update_usecs > FILEOP_STALLING_INTERVAL_US)
                stalled_msg = _("(stalled)");

            force_update = (tv_current - tctx->transfer_start) > FILEOP_UPDATE_INTERVAL_US;

            if (verbose && ctx->dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
            {
                file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
                file_progress_show_total (tctx, ctx, tctx->copied_bytes, force_update);
            }

            file_progress_show (ctx, file_part + ctx->do_reget, file_size, stalled_msg,
                                force_update);
            mc_refresh ();

            return_status = check_progress_buttons (ctx);
            if (return_status != FILE_CONT)
            {
                int query_res;

                query_res =
                    query_dialog (Q_ ("DialogTitle|Copy"),
                                  _("Incomplete file was retrieved"), D_ERROR, 3,
                                  _("&Delete"), _("&Keep"), _("&Continue copy"));

                switch (query_res)
                {
                case 0:
                    /* delete */
                    dst_status = DEST_SHORT_DELETE;
                    goto ret;

                case 1:
                    /* keep */
                    dst_status = DEST_SHORT_KEEP;
                    goto ret;

                default:
                    /* continue copy */
                    break;
                }
            }
        }

        /* copy successful */
        dst_status = DEST_FULL;
    }

  ret:
    g_free (buf);

    rotate_dash (FALSE);
    while (src_desc != -1 && mc_close (src_desc) < 0 && !ctx->skip_all)
    {
        temp_status = file_error (TRUE, _("Cannot close source file \"%s\"\n%s"), src_path);
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
        temp_status = file_error (TRUE, _("Cannot close target file \"%s\"\n%s"), dst_path);
        if (temp_status == FILE_RETRY)
            continue;
        if (temp_status == FILE_SKIPALL)
            ctx->skip_all = TRUE;
        return_status = temp_status;
        break;
    }

    if (dst_status == DEST_SHORT_QUERY)
    {
        /* Query to remove short file */
        if (query_dialog (Q_ ("DialogTitle|Copy"), _("Incomplete file was retrieved"),
                          D_ERROR, 2, _("&Delete"), _("&Keep")) == 0)
            dst_status = DEST_SHORT_DELETE;
        else
            dst_status = DEST_SHORT_KEEP;
    }

    if (dst_status == DEST_SHORT_DELETE)
        mc_unlink (dst_vpath);
    else if (dst_status == DEST_FULL && !appending)
    {
        /* Copy has succeeded */

        while (ctx->preserve_uidgid && mc_chown (dst_vpath, src_uid, src_gid) != 0
               && !ctx->skip_all)
        {
            temp_status = file_error (TRUE, _("Cannot chown target file \"%s\"\n%s"), dst_path);
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

        while (ctx->preserve && mc_chmod (dst_vpath, (src_mode & ctx->umask_kill)) != 0
               && !ctx->skip_all)
        {
            temp_status = file_error (TRUE, _("Cannot chmod target file \"%s\"\n%s"), dst_path);
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

        if (!ctx->preserve && !dst_exists)
        {
            src_mode = umask (-1);
            umask (src_mode);
            src_mode = 0100666 & ~src_mode;
            mc_chmod (dst_vpath, (src_mode & ctx->umask_kill));
        }
    }

    /* Always sync timestamps */
    if (dst_status == DEST_FULL || dst_status == DEST_SHORT_KEEP)
        mc_utime (dst_vpath, &times);

    if (return_status == FILE_CONT)
        return_status = progress_update_one (tctx, ctx, file_size);

  ret_fast:
    vfs_path_free (src_vpath, TRUE);
    vfs_path_free (dst_vpath, TRUE);
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
copy_dir_dir (file_op_total_context_t * tctx, file_op_context_t * ctx, const char *s, const char *d,
              gboolean toplevel, gboolean move_over, gboolean do_delete, GSList * parent_dirs)
{
    struct vfs_dirent *next;
    struct stat dst_stat, src_stat;
    DIR *reading;
    FileProgressStatus return_status = FILE_CONT;
    struct link *lp;
    vfs_path_t *src_vpath, *dst_vpath;
    gboolean do_mkdir = TRUE;

    src_vpath = vfs_path_from_str (s);
    dst_vpath = vfs_path_from_str (d);

    /* First get the mode of the source dir */

  retry_src_stat:
    if ((*ctx->stat_func) (src_vpath, &src_stat) != 0)
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Cannot stat source directory \"%s\"\n%s"), s);
            if (return_status == FILE_RETRY)
                goto retry_src_stat;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret_fast;
    }

    if (is_in_linklist (dest_dirs, src_vpath, &src_stat) != NULL)
    {
        /* Don't copy a directory we created before (we don't want to copy
           infinitely if a directory is copied into itself) */
        /* FIXME: should there be an error message and FILE_SKIP? - Norbert */
        return_status = FILE_CONT;
        goto ret_fast;
    }

    /* Hmm, hardlink to directory??? - Norbert */
    /* FIXME: In this step we should do something in case the destination already exist */
    /* Check the hardlinks */
    if (ctx->preserve)
    {
        switch (check_hardlinks (src_vpath, &src_stat, dst_vpath, &ctx->skip_all))
        {
        case HARDLINK_OK:
            /* We have made a hardlink - no more processing is necessary */
            goto ret_fast;

        case HARDLINK_ABORT:
            return_status = FILE_ABORT;
            goto ret_fast;

        default:
            break;
        }
    }

    if (!S_ISDIR (src_stat.st_mode))
    {
        if (ctx->skip_all)
            return_status = FILE_SKIPALL;
        else
        {
            return_status = file_error (TRUE, _("Source \"%s\" is not a directory\n%s"), s);
            if (return_status == FILE_RETRY)
                goto retry_src_stat;
            if (return_status == FILE_SKIPALL)
                ctx->skip_all = TRUE;
        }
        goto ret_fast;
    }

    if (is_in_linklist (parent_dirs, src_vpath, &src_stat) != NULL)
    {
        /* we found a cyclic symbolic link */
        message (D_ERROR, MSG_ERROR, _("Cannot copy cyclic symbolic link\n\"%s\""), s);
        return_status = FILE_SKIP;
        goto ret_fast;
    }

    lp = g_new0 (struct link, 1);
    lp->vfs = vfs_path_get_last_path_vfs (src_vpath);
    lp->ino = src_stat.st_ino;
    lp->dev = src_stat.st_dev;
    parent_dirs = g_slist_prepend (parent_dirs, lp);

  retry_dst_stat:
    /* Now, check if the dest dir exists, if not, create it. */
    if (mc_stat (dst_vpath, &dst_stat) != 0)
    {
        /* Here the dir doesn't exist : make it ! */
        if (move_over && mc_rename (src_vpath, dst_vpath) == 0)
        {
            return_status = FILE_CONT;
            goto ret;
        }
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
        if (!S_ISDIR (dst_stat.st_mode))
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (TRUE, _("Destination \"%s\" must be a directory\n%s"), d);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
                if (return_status == FILE_RETRY)
                    goto retry_dst_stat;
            }
            goto ret;
        }
        /* Dive into subdir if exists */
        if (toplevel && ctx->dive_into_subdirs)
        {
            vfs_path_t *tmp;

            tmp = dst_vpath;
            dst_vpath = vfs_path_append_new (dst_vpath, x_basename (s), (char *) NULL);
            vfs_path_free (tmp, TRUE);

        }
        else
            do_mkdir = FALSE;
    }

    d = vfs_path_as_str (dst_vpath);

    if (do_mkdir)
    {
        while (my_mkdir (dst_vpath, (src_stat.st_mode & ctx->umask_kill) | S_IRWXU) != 0)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status =
                    file_error (TRUE, _("Cannot create target directory \"%s\"\n%s"), d);
                if (return_status == FILE_SKIPALL)
                    ctx->skip_all = TRUE;
            }
            if (return_status != FILE_RETRY)
                goto ret;
        }

        lp = g_new0 (struct link, 1);
        mc_stat (dst_vpath, &dst_stat);
        lp->vfs = vfs_path_get_last_path_vfs (dst_vpath);
        lp->ino = dst_stat.st_ino;
        lp->dev = dst_stat.st_dev;
        dest_dirs = g_slist_prepend (dest_dirs, lp);
    }

    if (ctx->preserve_uidgid)
    {
        while (mc_chown (dst_vpath, src_stat.st_uid, src_stat.st_gid) != 0)
        {
            if (ctx->skip_all)
                return_status = FILE_SKIPALL;
            else
            {
                return_status = file_error (TRUE, _("Cannot chown target directory \"%s\"\n%s"), d);
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
        if (DIR_IS_DOT (next->d_name) || DIR_IS_DOTDOT (next->d_name))
            continue;

        /* get the filename and add it to the src directory */
        path = mc_build_filename (s, next->d_name, (char *) NULL);
        tmp_vpath = vfs_path_from_str (path);

        (*ctx->stat_func) (tmp_vpath, &dst_stat);
        if (S_ISDIR (dst_stat.st_mode))
        {
            char *mdpath;

            mdpath = mc_build_filename (d, next->d_name, (char *) NULL);
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

            dest_file = mc_build_filename (d, x_basename (path), (char *) NULL);
            return_status = copy_file_file (tctx, ctx, path, dest_file);
            g_free (dest_file);
        }

        g_free (path);

        if (do_delete && return_status == FILE_CONT)
        {
            if (ctx->erase_at_end)
            {
                if (erase_list == NULL)
                    erase_list = g_queue_new ();

                lp = g_new0 (struct link, 1);
                lp->src_vpath = tmp_vpath;
                lp->st_mode = dst_stat.st_mode;
                g_queue_push_tail (erase_list, lp);
                tmp_vpath = NULL;
            }
            else if (S_ISDIR (dst_stat.st_mode))
                return_status = erase_dir_iff_empty (ctx, tmp_vpath, tctx->progress_count);
            else
                return_status = erase_file (tctx, ctx, tmp_vpath);
        }
        vfs_path_free (tmp_vpath, TRUE);
    }
    mc_closedir (reading);

    if (ctx->preserve)
    {
        mc_timesbuf_t times;

        mc_chmod (dst_vpath, src_stat.st_mode & ctx->umask_kill);
        get_times (&src_stat, &times);
        mc_utime (dst_vpath, &times);
    }
    else
    {
        src_stat.st_mode = umask (-1);
        umask (src_stat.st_mode);
        src_stat.st_mode = 0100777 & ~src_stat.st_mode;
        mc_chmod (dst_vpath, src_stat.st_mode & ctx->umask_kill);
    }

  ret:
    free_link (parent_dirs->data);
    g_slist_free_1 (parent_dirs);
  ret_fast:
    vfs_path_free (src_vpath, TRUE);
    vfs_path_free (dst_vpath, TRUE);
    return return_status;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Move routines */

FileProgressStatus
move_dir_dir (file_op_total_context_t * tctx, file_op_context_t * ctx, const char *s, const char *d)
{
    return do_move_dir_dir (NULL, tctx, ctx, s, d);
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Erase routines */

FileProgressStatus
erase_dir (file_op_total_context_t * tctx, file_op_context_t * ctx, const vfs_path_t * vpath)
{
    file_progress_show_deleting (ctx, vfs_path_as_str (vpath), NULL);
    file_progress_show_count (ctx, tctx->progress_count, ctx->progress_count);
    if (check_progress_buttons (ctx) == FILE_ABORT)
        return FILE_ABORT;

    mc_refresh ();

    /* The old way to detect a non empty directory was:
       error = my_rmdir (s);
       if (error && (errno == ENOTEMPTY || errno == EEXIST))){
       For the linux user space nfs server (nfs-server-2.2beta29-2)
       we would have to check also for EIO. I hope the new way is
       fool proof. (Norbert)
     */
    if (check_dir_is_empty (vpath) == 0)
    {                           /* not empty */
        FileProgressStatus error;

        error = query_recursive (ctx, vfs_path_as_str (vpath));
        if (error == FILE_CONT)
            error = recursive_erase (tctx, ctx, vpath);
        return error;
    }

    return try_erase_dir (ctx, vfs_path_as_str (vpath));
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Panel operate routines */

void
dirsize_status_init_cb (status_msg_t * sm)
{
    dirsize_status_msg_t *dsm = (dirsize_status_msg_t *) sm;
    WGroup *gd = GROUP (sm->dlg);
    Widget *wd = WIDGET (sm->dlg);
    WRect r = wd->rect;

    const char *b1_name = N_("&Abort");
    const char *b2_name = N_("&Skip");
    int b_width, ui_width;

#ifdef ENABLE_NLS
    b1_name = _(b1_name);
    b2_name = _(b2_name);
#endif

    b_width = str_term_width1 (b1_name) + 4;
    if (dsm->allow_skip)
        b_width += str_term_width1 (b2_name) + 4 + 1;

    ui_width = MAX (COLS / 2, b_width + 6);
    dsm->dirname = label_new (2, 3, NULL);
    group_add_widget (gd, dsm->dirname);
    dsm->count_size = label_new (3, 3, NULL);
    group_add_widget (gd, dsm->count_size);
    group_add_widget (gd, hline_new (4, -1, -1));

    dsm->abort_button = WIDGET (button_new (5, 3, FILE_ABORT, NORMAL_BUTTON, b1_name, NULL));
    group_add_widget (gd, dsm->abort_button);
    if (dsm->allow_skip)
    {
        dsm->skip_button = WIDGET (button_new (5, 3, FILE_SKIP, NORMAL_BUTTON, b2_name, NULL));
        group_add_widget (gd, dsm->skip_button);
        widget_select (dsm->skip_button);
    }

    r.lines = 8;
    r.cols = ui_width;
    widget_set_size_rect (wd, &r);
    dirsize_status_locate_buttons (dsm);
}

/* --------------------------------------------------------------------------------------------- */

int
dirsize_status_update_cb (status_msg_t * sm)
{
    dirsize_status_msg_t *dsm = (dirsize_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    WRect r = wd->rect;

    /* update second (longer label) */
    label_set_textv (dsm->count_size, _("Directories: %zu, total size: %s"),
                     dsm->dir_count, size_trunc_sep (dsm->total_size, panels_options.kilobyte_si));

    /* enlarge dialog if required */
    if (WIDGET (dsm->count_size)->rect.cols + 6 > r.cols)
    {
        r.cols = WIDGET (dsm->count_size)->rect.cols + 6;
        widget_set_size_rect (wd, &r);
        dirsize_status_locate_buttons (dsm);
        widget_draw (wd);
        /* TODO: ret rid of double redraw */
    }

    /* adjust first label */
    label_set_text (dsm->dirname,
                    str_trunc (vfs_path_as_str (dsm->dirname_vpath), wd->rect.cols - 6));

    switch (status_msg_common_update (sm))
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

void
dirsize_status_deinit_cb (status_msg_t * sm)
{
    (void) sm;

    /* schedule to update passive panel */
    if (get_other_type () == view_listing)
        other_panel->dirty = TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * compute_dir_size:
 *
 * Computes the number of bytes used by the files in a directory
 */

FileProgressStatus
compute_dir_size (const vfs_path_t * dirname_vpath, dirsize_status_msg_t * sm,
                  size_t * ret_dir_count, size_t * ret_marked_count, uintmax_t * ret_total,
                  gboolean follow_symlinks)
{
    return do_compute_dir_size (dirname_vpath, sm, ret_dir_count, ret_marked_count, ret_total,
                                follow_symlinks ? mc_stat : mc_lstat);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * panel_operate:
 *
 * Performs one of the operations on the current on the source_panel
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
    WPanel *panel = PANEL (source_panel);
    const gboolean single_entry = force_single || (panel->marked <= 1)
        || (get_current_type () == view_tree);

    const char *source = NULL;
    char *dest = NULL;
    vfs_path_t *dest_vpath = NULL;
    vfs_path_t *save_cwd = NULL, *save_dest = NULL;
    struct stat src_stat;
    gboolean ret_val = TRUE;
    int i;
    FileProgressStatus value;
    file_op_context_t *ctx;
    file_op_total_context_t *tctx;
    filegui_dialog_type_t dialog_type = FILEGUI_DIALOG_ONE_ITEM;

    gboolean do_bg = FALSE;     /* do background operation? */

    static gboolean i18n_flag = FALSE;
    if (!i18n_flag)
    {
        for (i = G_N_ELEMENTS (op_names); i-- != 0;)
            op_names[i] = Q_ (op_names[i]);
        i18n_flag = TRUE;
    }

    linklist = free_linklist (linklist);
    dest_dirs = free_linklist (dest_dirs);

    save_cwds_stat ();

    if (single_entry)
    {
        source = check_single_entry (panel, force_single, &src_stat);

        if (source == NULL)
            return FALSE;
    }

    ctx = file_op_context_new (operation);

    /* Show confirmation dialog */
    if (operation != OP_DELETE)
    {
        dest = do_confirm_copy_move (panel, force_single, source, &src_stat, ctx, &do_bg);
        if (dest == NULL)
        {
            ret_val = FALSE;
            goto ret_fast;
        }

        dest_vpath = vfs_path_from_str (dest);
    }
    else if (confirm_delete && !do_confirm_erase (panel, source, &src_stat))
    {
        ret_val = FALSE;
        goto ret_fast;
    }

    tctx = file_op_total_context_new ();
    tctx->transfer_start = g_get_monotonic_time ();

#ifdef ENABLE_BACKGROUND
    /* Did the user select to do a background operation? */
    if (do_bg)
    {
        int v;

        v = do_background (ctx,
                           g_strconcat (op_names[operation], ": ",
                                        vfs_path_as_str (panel->cwd_vpath), (char *) NULL));
        if (v == -1)
            message (D_ERROR, MSG_ERROR, _("Sorry, I could not put the job in background"));

        /* If we are the parent */
        if (v == 1)
        {
            mc_setctl (panel->cwd_vpath, VFS_SETCTL_FORGET, NULL);

            mc_setctl (dest_vpath, VFS_SETCTL_FORGET, NULL);
            vfs_path_free (dest_vpath, TRUE);
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
        else if (single_entry && S_ISDIR (panel_current_entry (panel)->st.st_mode))
            dialog_type = FILEGUI_DIALOG_MULTI_ITEM;
        else if (single_entry || force_single)
            dialog_type = FILEGUI_DIALOG_ONE_ITEM;
        else
            dialog_type = FILEGUI_DIALOG_MULTI_ITEM;
    }

    /* Initialize things */
    /* We do not want to trash cache every time file is
       created/touched. However, this will make our cache contain
       invalid data. */
    if ((dest != NULL)
        && (mc_setctl (dest_vpath, VFS_SETCTL_STALE_DATA, GUINT_TO_POINTER (1)) != 0))
        save_dest = vfs_path_from_str (dest);

    if ((vfs_path_tokens_count (panel->cwd_vpath) != 0)
        && (mc_setctl (panel->cwd_vpath, VFS_SETCTL_STALE_DATA, GUINT_TO_POINTER (1)) != 0))
        save_cwd = vfs_path_clone (panel->cwd_vpath);

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
            vfs_path_free (vpath, TRUE);
            if (chdir_retcode < 0)
            {
                ret_val = FALSE;
                goto clean_up;
            }
        }

        value = operate_single_file (panel, tctx, ctx, source, &src_stat, dest, dialog_type);
        if ((value == FILE_CONT) && !force_single)
            unmark_files (panel);
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
                || file_error (TRUE, _("Destination \"%s\" must be a directory\n%s"),
                               dest) != FILE_RETRY)
                goto clean_up;
        }

        /* TODO: the good way is required to skip directories scanning in case of rename/move
         * of several directories. Since reqular expression can be used for destination,
         * some directory movements can be a cross-filesystem and directory scanning is useful
         * for those directories only. */

        if (panel_operate_init_totals (panel, NULL, NULL, ctx, file_op_compute_totals, dialog_type)
            == FILE_CONT)
        {
            /* Loop for every file, perform the actual copy operation */
            for (i = 0; i < panel->dir.len; i++)
            {
                const char *source2;

                if (panel->dir.list[i].f.marked == 0)
                    continue;   /* Skip the unmarked ones */

                source2 = panel->dir.list[i].fname->str;
                src_stat = panel->dir.list[i].st;

                value = operate_one_file (panel, tctx, ctx, source2, &src_stat, dest);

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
        mc_setctl (save_cwd, VFS_SETCTL_STALE_DATA, NULL);
        vfs_path_free (save_cwd, TRUE);
    }

    if (save_dest != NULL)
    {
        mc_setctl (save_dest, VFS_SETCTL_STALE_DATA, NULL);
        vfs_path_free (save_dest, TRUE);
    }

    linklist = free_linklist (linklist);
    dest_dirs = free_linklist (dest_dirs);
    g_free (dest);
    vfs_path_free (dest_vpath, TRUE);
    MC_PTR_FREE (ctx->dest_mask);

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

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();

    return ret_val;
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */
/* {{{ Query/status report routines */
/** Report error with one file */
FileProgressStatus
file_error (gboolean allow_retry, const char *format, const char *file)
{
    char buf[BUF_MEDIUM];

    g_snprintf (buf, sizeof (buf), format, path_trunc (file, 30), unix_error_string (errno));

    return do_file_error (allow_retry, buf);
}

/* --------------------------------------------------------------------------------------------- */

/*
   Cause emacs to enter folding mode for this file:
   Local variables:
   end:
 */
