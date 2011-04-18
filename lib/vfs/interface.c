/* Virtual File System: interface functions
   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

/**
 * \file
 * \brief Source: Virtual File System: path handlers
 * \author Slava Zanko
 * \date 2011
 */


#include <config.h>

#include <stdio.h>
#include <stdlib.h>             /* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>              /* is_digit() */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "lib/global.h"

#include "lib/widget.h"         /* message() */
#include "lib/strutil.h"        /* str_crt_conv_from() */

#include "vfs.h"
#include "utilvfs.h"
#include "path.h"
#include "gc.h"

extern GString *vfs_str_buffer;
extern struct vfs_class *current_vfs;
extern char *current_dir;

/*** global variables ****************************************************************************/

struct dirent *mc_readdir_result = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

struct vfs_dirinfo
{
    DIR *info;
    GIConv converter;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
mc_def_getlocalcopy (const char *filename)
{
    char *tmp;
    int fdin, fdout;
    ssize_t i;
    char buffer[8192];
    struct stat mystat;

    fdin = mc_open (filename, O_RDONLY | O_LINEAR);
    if (fdin == -1)
        return NULL;

    fdout = vfs_mkstemps (&tmp, "vfs", filename);

    if (fdout == -1)
        goto fail;
    while ((i = mc_read (fdin, buffer, sizeof (buffer))) > 0)
    {
        if (write (fdout, buffer, i) != i)
            goto fail;
    }
    if (i == -1)
        goto fail;
    i = mc_close (fdin);
    fdin = -1;
    if (i == -1)
        goto fail;
    if (close (fdout) == -1)
    {
        fdout = -1;
        goto fail;
    }

    if (mc_stat (filename, &mystat) != -1)
        chmod (tmp, mystat.st_mode);

    return tmp;

  fail:
    if (fdout != -1)
        close (fdout);
    if (fdin != -1)
        mc_close (fdin);
    g_free (tmp);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
mc_def_ungetlocalcopy (struct vfs_class *vfs, const char *filename,
                       const char *local, int has_changed)
{
    int fdin = -1, fdout = -1;
    if (has_changed)
    {
        char buffer[8192];
        ssize_t i;

        if (!vfs->write)
            goto failed;

        fdin = open (local, O_RDONLY);
        if (fdin == -1)
            goto failed;
        fdout = mc_open (filename, O_WRONLY | O_TRUNC);
        if (fdout == -1)
            goto failed;
        while ((i = read (fdin, buffer, sizeof (buffer))) > 0)
            if (mc_write (fdout, buffer, (size_t) i) != i)
                goto failed;
        if (i == -1)
            goto failed;

        if (close (fdin) == -1)
        {
            fdin = -1;
            goto failed;
        }
        fdin = -1;
        if (mc_close (fdout) == -1)
        {
            fdout = -1;
            goto failed;
        }
    }
    unlink (local);
    return 0;

  failed:
    message (D_ERROR, _("Changes to file lost"), "%s", filename);
    if (fdout != -1)
        mc_close (fdout);
    if (fdin != -1)
        close (fdin);
    unlink (local);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
mc_open (const char *filename, int flags, ...)
{
    int mode = 0, result = -1;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (filename);
    if (vpath == NULL)
        return -1;

    /* Get the mode flag */
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start (ap, flags);
        mode = va_arg (ap, int);
        va_end (ap);
    }

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);
    if (path_element != NULL && path_element->class->open != NULL)
    {
        void *info;
        /* open must be supported */
        info = path_element->class->open (vpath, flags, mode);
        if (info == NULL)
            errno = vfs_ferrno (path_element->class);
        else
            result = vfs_new_handle (path_element->class, info);
    }
    else
        errno = -EOPNOTSUPP;

    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */

#define MC_NAMEOP(name, inarg, callarg) \
int mc_##name inarg \
{ \
    int result; \
    vfs_path_t *vpath; \
    vfs_path_element_t *path_element; \
\
    vpath = vfs_path_from_str (path); \
    if (vpath == NULL) \
        return -1; \
\
    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1); \
    if (path_element == NULL) \
    { \
        vfs_path_free(vpath); \
        return -1; \
    } \
\
    if (path_element->class == NULL) \
    { \
        vfs_path_free(vpath); \
        return -1; \
    } \
\
    result = path_element->class != NULL ? path_element->class->name callarg : -1; \
    if (result == -1) \
        errno = path_element->class->name != NULL ? vfs_ferrno (path_element->class) : E_NOTSUPP; \
    vfs_path_free(vpath); \
    return result; \
}

MC_NAMEOP (chmod, (const char *path, mode_t mode), (path_element->class, vpath->unparsed, mode))
MC_NAMEOP (chown, (const char *path, uid_t owner, gid_t group), (path_element->class, vpath->unparsed, owner, group))
MC_NAMEOP (utime, (const char *path, struct utimbuf * times), (path_element->class, vpath->unparsed, times))
MC_NAMEOP (readlink, (const char *path, char *buf, size_t bufsiz), (path_element->class, vpath->unparsed, buf, bufsiz))
MC_NAMEOP (unlink, (const char *path), (path_element->class, vpath->unparsed))
MC_NAMEOP (mkdir, (const char *path, mode_t mode), (path_element->class, vpath->unparsed, mode))
MC_NAMEOP (rmdir, (const char *path), (path_element->class, vpath->unparsed))
MC_NAMEOP (mknod, (const char *path, mode_t mode, dev_t dev), (path_element->class, vpath->unparsed, mode, dev))

/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
mc_symlink (const char *name1, const char *path)
{
    int result = -1;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;
    char *lpath;
    char *tmp;

    vpath = vfs_path_from_str (path);
    if (vpath == NULL)
        return -1;

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);
    if (path_element == NULL)
    {
        vfs_path_free (vpath);
        return -1;
    }

    tmp = g_strdup (name1);
    lpath = vfs_translate_path_n (tmp);
    g_free (tmp);

    if (lpath != NULL)
    {
        result =
            path_element->class->symlink !=
            NULL ? path_element->class->symlink (path_element->class, lpath, vpath->unparsed) : -1;
        if (result == -1)
            errno =
                path_element->class->symlink != NULL ? vfs_ferrno (path_element->class) : E_NOTSUPP;
    }
    g_free (lpath);
    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */

#define MC_HANDLEOP(name, inarg, callarg) \
ssize_t mc_##name inarg \
{ \
    struct vfs_class *vfs; \
    int result; \
    if (handle == -1) \
        return -1; \
    vfs = vfs_class_find_by_handle (handle); \
    if (vfs == NULL) \
        return -1; \
    result = vfs->name != NULL ? vfs->name callarg : -1; \
    if (result == -1) \
        errno = vfs->name != NULL ? vfs_ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_HANDLEOP (read, (int handle, void *buffer, size_t count), (vfs_class_data_find_by_handle (handle), buffer, count))
MC_HANDLEOP (write, (int handle, const void *buf, size_t nbyte), (vfs_class_data_find_by_handle (handle), buf, nbyte))

/* --------------------------------------------------------------------------------------------- */

#define MC_RENAMEOP(name) \
int mc_##name (const char *fname1, const char *fname2) \
{ \
    int result; \
    vfs_path_t *vpath1, *vpath2; \
    vfs_path_element_t *path_element1, *path_element2; \
\
    vpath1 = vfs_path_from_str (fname1); \
    if (vpath1 == NULL) \
        return -1; \
\
    vpath2 = vfs_path_from_str (fname2); \
    if (vpath2 == NULL) \
    { \
        vfs_path_free(vpath1); \
        return -1; \
    }\
    path_element1 = vfs_path_get_by_index (vpath1, vfs_path_length (vpath1) - 1); \
    path_element2 = vfs_path_get_by_index (vpath2, vfs_path_length (vpath2) - 1); \
\
    if (path_element1->class != path_element2->class) \
    { \
        errno = EXDEV; \
        vfs_path_free(vpath1); \
        vfs_path_free(vpath2); \
        return -1; \
    }\
\
    result = path_element1->class->name != NULL \
        ? path_element1->class->name (path_element1->class, vpath1->unparsed, vpath2->unparsed) \
        : -1; \
    if (result == -1) \
        errno = path_element1->class->name != NULL ? vfs_ferrno (path_element1->class) : E_NOTSUPP; \
    vfs_path_free(vpath1); \
    vfs_path_free(vpath2); \
    return result; \
}

MC_RENAMEOP (link)
MC_RENAMEOP (rename)

/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
mc_ctl (int handle, int ctlop, void *arg)
{
    struct vfs_class *vfs = vfs_class_find_by_handle (handle);

    if (vfs == NULL)
        return 0;

    return vfs->ctl ? (*vfs->ctl) (vfs_class_data_find_by_handle (handle), ctlop, arg) : 0;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_setctl (const char *path, int ctlop, void *arg)
{
    int result = -1;

    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (path);
    if (vpath == NULL)
        vfs_die ("You don't want to pass NULL to mc_setctl.");

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);
    if (path_element != NULL && path_element->class != NULL)
        result =
            path_element->class->setctl != NULL ? path_element->class->setctl (path_element->class,
                                                                               vpath->unparsed,
                                                                               ctlop, arg) : 0;

    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_close (int handle)
{
    struct vfs_class *vfs;
    int result;

    if (handle == -1 || !vfs_class_data_find_by_handle (handle))
        return -1;

    vfs = vfs_class_find_by_handle (handle);
    if (vfs == NULL)
        return -1;

    if (handle < 3)
        return close (handle);

    if (!vfs->close)
        vfs_die ("VFS must support close.\n");
    result = (*vfs->close) (vfs_class_data_find_by_handle (handle));
    vfs_free_handle (handle);
    if (result == -1)
        errno = vfs_ferrno (vfs);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

DIR *
mc_opendir (const char *dirname)
{
    int handle, *handlep;
    void *info;
    struct vfs_class *vfs;
    char *canon;
    char *dname;
    struct vfs_dirinfo *dirinfo;
    const char *encoding;

    canon = vfs_canon (dirname);
    dname = vfs_translate_path_n (canon);

    if (dname != NULL)
    {
        vfs = vfs_get_class (dname);
        info = vfs->opendir ? (*vfs->opendir) (vfs, dname) : NULL;
        g_free (dname);

        if (info == NULL)
        {
            errno = vfs->opendir ? vfs_ferrno (vfs) : E_NOTSUPP;
            g_free (canon);
            return NULL;
        }

        dirinfo = g_new (struct vfs_dirinfo, 1);
        dirinfo->info = info;

        encoding = vfs_get_encoding (canon);
        g_free (canon);
        dirinfo->converter = (encoding != NULL) ? str_crt_conv_from (encoding) : str_cnv_from_term;
        if (dirinfo->converter == INVALID_CONV)
            dirinfo->converter = str_cnv_from_term;

        handle = vfs_new_handle (vfs, dirinfo);

        handlep = g_new (int, 1);
        *handlep = handle;
        return (DIR *) handlep;
    }
    else
    {
        g_free (canon);
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

struct dirent *
mc_readdir (DIR * dirp)
{
    int handle;
    struct vfs_class *vfs;
    struct dirent *entry = NULL;
    struct vfs_dirinfo *dirinfo;
    estr_t state;

    if (!mc_readdir_result)
    {
        /* We can't just allocate struct dirent as (see man dirent.h)
         * struct dirent has VERY nonnaive semantics of allocating
         * d_name in it. Moreover, linux's glibc-2.9 allocates dirents _less_,
         * than 'sizeof (struct dirent)' making full bitwise (sizeof dirent) copy
         * heap corrupter. So, allocate longliving dirent with at least
         * (MAXNAMLEN + 1) for d_name in it.
         * Strictly saying resulting dirent is unusable as we don't adjust internal
         * structures, holding dirent size. But we don't use it in libc infrastructure.
         * TODO: to make simpler homemade dirent-alike structure.
         */
        mc_readdir_result = (struct dirent *) g_malloc (sizeof (struct dirent) + MAXNAMLEN + 1);
    }

    if (!dirp)
    {
        errno = EFAULT;
        return NULL;
    }
    handle = *(int *) dirp;

    vfs = vfs_class_find_by_handle (handle);
    if (vfs == NULL)
        return NULL;

    dirinfo = vfs_class_data_find_by_handle (handle);
    if (vfs->readdir)
    {
        entry = (*vfs->readdir) (dirinfo->info);
        if (entry == NULL)
            return NULL;
        g_string_set_size (vfs_str_buffer, 0);
        state = str_vfs_convert_from (dirinfo->converter, entry->d_name, vfs_str_buffer);
        mc_readdir_result->d_ino = entry->d_ino;
        g_strlcpy (mc_readdir_result->d_name, vfs_str_buffer->str, MAXNAMLEN + 1);
    }
    if (entry == NULL)
        errno = vfs->readdir ? vfs_ferrno (vfs) : E_NOTSUPP;
    return (entry != NULL) ? mc_readdir_result : NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_closedir (DIR * dirp)
{
    int handle = *(int *) dirp;
    struct vfs_class *vfs;
    int result = -1;

    vfs = vfs_class_find_by_handle (handle);
    if (vfs != NULL)
    {
        struct vfs_dirinfo *dirinfo;

        dirinfo = vfs_class_data_find_by_handle (handle);
        if (dirinfo->converter != str_cnv_from_term)
            str_close_conv (dirinfo->converter);

        result = vfs->closedir ? (*vfs->closedir) (dirinfo->info) : -1;
        vfs_free_handle (handle);
        g_free (dirinfo);
    }
    g_free (dirp);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_stat (const char *filename, struct stat *buf)
{
    int result = -1;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (filename);
    if (vpath == NULL)
        return -1;

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);

    if (path_element != NULL && path_element->class != NULL)
    {
        result =
            path_element->class->stat ? (*path_element->class->stat) (path_element->class,
                                                                      vpath->unparsed, buf) : -1;
        if (result == -1)
            errno = path_element->class->name ? vfs_ferrno (path_element->class) : E_NOTSUPP;
    }

    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_lstat (const char *filename, struct stat *buf)
{
    int result = -1;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (filename);
    if (vpath == NULL)
        return -1;

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);

    if (path_element != NULL && path_element->class != NULL)
    {
        result =
            path_element->class->lstat ? (*path_element->class->lstat) (path_element->class,
                                                                        vpath->unparsed, buf) : -1;
        if (result == -1)
            errno = path_element->class->name ? vfs_ferrno (path_element->class) : E_NOTSUPP;
    }

    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_fstat (int handle, struct stat *buf)
{
    struct vfs_class *vfs;
    int result;

    if (handle == -1)
        return -1;

    vfs = vfs_class_find_by_handle (handle);
    if (vfs == NULL)
        return -1;

    result = vfs->fstat ? (*vfs->fstat) (vfs_class_data_find_by_handle (handle), buf) : -1;
    if (result == -1)
        errno = vfs->name ? vfs_ferrno (vfs) : E_NOTSUPP;
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return current directory. If it's local, reread the current directory
 * from the OS. Put directory to the provided buffer.
 */

char *
mc_get_current_wd (char *buffer, size_t size)
{
    const char *cwd = _vfs_get_cwd ();

    g_strlcpy (buffer, cwd, size);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_getlocalcopy (const char *pathname)
{
    char *result = NULL;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (pathname);
    if (vpath == NULL)
        return NULL;

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);
    if (path_element != NULL)
    {
        result = path_element->class->getlocalcopy != NULL ?
            path_element->class->getlocalcopy (vpath) : mc_def_getlocalcopy (vpath->unparsed);
        if (result == NULL)
            errno = vfs_ferrno (path_element->class);
    }
    vfs_path_free (vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed)
{
    int return_value = -1;
    vfs_path_t *vpath;
    vfs_path_element_t *path_element;

    vpath = vfs_path_from_str (pathname);
    if (vpath == NULL)
        return -1;

    path_element = vfs_path_get_by_index (vpath, vfs_path_length (vpath) - 1);
    if (path_element != NULL)
    {
        return_value = path_element->class->ungetlocalcopy != NULL ?
            path_element->class->ungetlocalcopy (vpath, local,
                                                 has_changed) :
            mc_def_ungetlocalcopy (path_element->class, vpath->unparsed, local, has_changed);
    }
    vfs_path_free (vpath);
    return return_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * VFS chdir.
 * Return 0 on success, -1 on failure.
 */

int
mc_chdir (const char *path)
{
    char *new_dir;
    char *trans_dir;
    struct vfs_class *old_vfs, *new_vfs;
    vfsid old_vfsid;
    int result;

    new_dir = vfs_canon (path);
    trans_dir = vfs_translate_path_n (new_dir);
    if (trans_dir != NULL)
    {
        new_vfs = vfs_get_class (trans_dir);
        if (!new_vfs->chdir)
        {
            g_free (new_dir);
            g_free (trans_dir);
            return -1;
        }

        result = (*new_vfs->chdir) (new_vfs, trans_dir);

        if (result == -1)
        {
            errno = vfs_ferrno (new_vfs);
            g_free (new_dir);
            g_free (trans_dir);
            return -1;
        }

        old_vfsid = vfs_getid (current_vfs, current_dir);
        old_vfs = current_vfs;

        /* Actually change directory */
        g_free (current_dir);
        current_dir = new_dir;
        current_vfs = new_vfs;

        /* This function uses the new current_dir implicitly */
        vfs_stamp_create (old_vfs, old_vfsid);

        /* Sometimes we assume no trailing slash on cwd */
        if (*current_dir)
        {
            char *p;
            p = strchr (current_dir, 0) - 1;
            if (*p == PATH_SEP && p > current_dir)
                *p = 0;
        }

        g_free (trans_dir);
        return 0;
    }
    else
    {
        g_free (new_dir);
        return -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

off_t
mc_lseek (int fd, off_t offset, int whence)
{
    struct vfs_class *vfs;
    off_t result;

    if (fd == -1)
        return -1;

    vfs = vfs_class_find_by_handle (fd);
    if (vfs == NULL)
        return -1;

    result = vfs->lseek ? (*vfs->lseek) (vfs_class_data_find_by_handle (fd), offset, whence) : -1;
    if (result == -1)
        errno = vfs->lseek ? vfs_ferrno (vfs) : E_NOTSUPP;
    return result;
}

/* --------------------------------------------------------------------------------------------- */
