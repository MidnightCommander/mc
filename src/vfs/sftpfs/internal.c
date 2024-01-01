/* Virtual File System: SFTP file system.
   The internal functions

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2011
   Slava Zanko <slavazanko@gmail.com>, 2011, 2012

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

#include <config.h>
#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "lib/global.h"
#include "lib/util.h"

#include "internal.h"

/*** global variables ****************************************************************************/

GString *sftpfs_filename_buffer = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Adjust block size and number of blocks */

static void
sftpfs_blksize (struct stat *s)
{
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    s->st_blksize = LIBSSH2_CHANNEL_WINDOW_DEFAULT;     /* FIXME */
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */
    vfs_adjust_stat (s);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Awaiting for any activity on socket.
 *
 * @param super extra data for SFTP connection
 * @param mcerror    pointer to the error object
 * @return 0 if success, negative value otherwise
 */

static int
sftpfs_internal_waitsocket (sftpfs_super_t * super, GError ** mcerror)
{
    struct timeval timeout = { 10, 0 };
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir, ret;

    mc_return_val_if_error (mcerror, -1);

    FD_ZERO (&fd);
    FD_SET (super->socket_handle, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions (super->session);

    if ((dir & LIBSSH2_SESSION_BLOCK_INBOUND) != 0)
        readfd = &fd;

    if ((dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0)
        writefd = &fd;

    ret = select (super->socket_handle + 1, readfd, writefd, NULL, &timeout);
    if (ret < 0)
    {
        int my_errno = errno;

        mc_propagate_error (mcerror, my_errno, _("sftp: socket error: %s"),
                            unix_error_string (my_errno));
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_stat_init (sftpfs_super_t ** super, const vfs_path_element_t ** path_element,
                  const vfs_path_t * vpath, GError ** mcerror, int stat_type,
                  LIBSSH2_SFTP_ATTRIBUTES * attrs)
{
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (super, path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename ((*path_element)->path);

    do
    {
        res = libssh2_sftp_stat_ex ((*super)->sftp_session, fixfname->str, fixfname->len,
                                    stat_type, attrs);
        if (res >= 0)
            break;

        if (sftpfs_is_sftp_error ((*super)->sftp_session, res, LIBSSH2_FX_PERMISSION_DENIED))
            return -EACCES;

        if (sftpfs_is_sftp_error ((*super)->sftp_session, res, LIBSSH2_FX_NO_SUCH_FILE))
            return -ENOENT;

        if (!sftpfs_waitsocket (*super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
sftpfs_waitsocket (sftpfs_super_t * super, int sftp_res, GError ** mcerror)
{
    if (sftp_res != LIBSSH2_ERROR_EAGAIN)
    {
        sftpfs_ssherror_to_gliberror (super, sftp_res, mcerror);
        return FALSE;
    }

    sftpfs_internal_waitsocket (super, mcerror);

    return (mcerror == NULL || *mcerror == NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
sftpfs_is_sftp_error (LIBSSH2_SFTP * sftp_session, int sftp_res, int sftp_error)
{
    return (sftp_res == LIBSSH2_ERROR_SFTP_PROTOCOL &&
            libssh2_sftp_last_error (sftp_session) == (unsigned long) sftp_error);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert libssh error to GError object.
 *
 * @param super   extra data for SFTP connection
 * @param libssh_errno errno from libssh
 * @param mcerror      pointer to the error object
 */

void
sftpfs_ssherror_to_gliberror (sftpfs_super_t * super, int libssh_errno, GError ** mcerror)
{
    char *err = NULL;
    int err_len;

    mc_return_if_error (mcerror);

    libssh2_session_last_error (super->session, &err, &err_len, 1);
    if (libssh_errno == LIBSSH2_ERROR_SFTP_PROTOCOL && super->sftp_session != NULL)
        mc_propagate_error (mcerror, libssh_errno, "%s %lu", err,
                            libssh2_sftp_last_error (super->sftp_session));
    else
        mc_propagate_error (mcerror, libssh_errno, "%s", err);
    g_free (err);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Fix filename for SFTP operations: add leading slash to file name.
 *
 * @param file_name file name
 * @param length length of returned string
 *
 * @return pointer to string that contains the file name with leading slash
 */

const GString *
sftpfs_fix_filename (const char *file_name)
{
    g_string_printf (sftpfs_filename_buffer, "%c%s", PATH_SEP, file_name);
    return sftpfs_filename_buffer;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
sftpfs_op_init (sftpfs_super_t ** super, const vfs_path_element_t ** path_element,
                const vfs_path_t * vpath, GError ** mcerror)
{
    struct vfs_s_super *lc_super = NULL;

    mc_return_val_if_error (mcerror, FALSE);

    if (vfs_s_get_path (vpath, &lc_super, 0) == NULL)
        return FALSE;

    if (lc_super == NULL)
        return FALSE;

    *super = SFTP_SUPER (lc_super);
    if ((*super)->sftp_session == NULL)
        return FALSE;

    *path_element = vfs_path_get_by_index (vpath, -1);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
sftpfs_attr_to_stat (const LIBSSH2_SFTP_ATTRIBUTES * attrs, struct stat *s)
{
    if ((attrs->flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        s->st_uid = attrs->uid;
        s->st_gid = attrs->gid;
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        s->st_atime = attrs->atime;
        s->st_mtime = attrs->mtime;
        s->st_ctime = attrs->mtime;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
        s->st_atim.tv_nsec = s->st_mtim.tv_nsec = s->st_ctim.tv_nsec = 0;
#endif
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
    {
        s->st_size = attrs->filesize;
        sftpfs_blksize (s);
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        s->st_mode = attrs->permissions;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a symbolic link.
 *
 * @param vpath   path to file, directory or symbolic link
 * @param buf     buffer for store stat-info
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_lstat (const vfs_path_t * vpath, struct stat *buf, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;

    res = sftpfs_stat_init (&super, &path_element, vpath, mcerror, LIBSSH2_SFTP_LSTAT, &attrs);
    if (res >= 0)
    {
        sftpfs_attr_to_stat (&attrs, buf);
        res = 0;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Getting information about a file or directory.
 *
 * @param vpath   path to file or directory
 * @param buf     buffer for store stat-info
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_stat (const vfs_path_t * vpath, struct stat *buf, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;

    res = sftpfs_stat_init (&super, &path_element, vpath, mcerror, LIBSSH2_SFTP_STAT, &attrs);
    if (res >= 0)
    {
        buf->st_nlink = 1;
        sftpfs_attr_to_stat (&attrs, buf);
        res = 0;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read value of a symbolic link.
 *
 * @param vpath   path to file or directory
 * @param buf     buffer for store stat-info
 * @param size    buffer size
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_readlink (const vfs_path_t * vpath, char *buf, size_t size, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (&super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res =
            libssh2_sftp_symlink_ex (super->sftp_session, fixfname->str, fixfname->len, buf, size,
                                     LIBSSH2_SFTP_READLINK);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create symlink to file or directory
 *
 * @param vpath1  path to file or directory
 * @param vpath2  path to symlink
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element2 = NULL;
    const char *path1;
    size_t path1_len;
    const GString *ctmp_path;
    char *tmp_path;
    unsigned int tmp_path_len;
    int res;

    if (!sftpfs_op_init (&super, &path_element2, vpath2, mcerror))
        return -1;

    ctmp_path = sftpfs_fix_filename (path_element2->path);
    tmp_path = g_strndup (ctmp_path->str, ctmp_path->len);
    tmp_path_len = ctmp_path->len;

    path1 = vfs_path_get_last_path_str (vpath1);
    path1_len = strlen (path1);

    do
    {
        res =
            libssh2_sftp_symlink_ex (super->sftp_session, path1, path1_len, tmp_path, tmp_path_len,
                                     LIBSSH2_SFTP_SYMLINK);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (super, res, mcerror))
        {
            g_free (tmp_path);
            return -1;
        }
    }
    while (res == LIBSSH2_ERROR_EAGAIN);
    g_free (tmp_path);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Changes the times of the file.
 *
 * @param vpath   path to file or directory
 * @param atime   access time
 * @param mtime   modification time
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_utime (const vfs_path_t * vpath, time_t atime, time_t mtime, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    const GString *fixfname;
    int res;

    res = sftpfs_stat_init (&super, &path_element, vpath, mcerror, LIBSSH2_SFTP_LSTAT, &attrs);
    if (res < 0)
        return res;

    attrs.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
    attrs.atime = atime;
    attrs.mtime = mtime;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res =
            libssh2_sftp_stat_ex (super->sftp_session, fixfname->str, fixfname->len,
                                  LIBSSH2_SFTP_SETSTAT, &attrs);
        if (res >= 0)
            break;

        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_NO_SUCH_FILE))
            return -ENOENT;

        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_FAILURE))
        {
            res = 0;            /* need something like ftpfs_ignore_chattr_errors */
            break;
        }

        if (!sftpfs_waitsocket (super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Changes the permissions of the file.
 *
 * @param vpath   path to file or directory
 * @param mode    mode (see man 2 open)
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_chmod (const vfs_path_t * vpath, mode_t mode, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    const GString *fixfname;
    int res;

    res = sftpfs_stat_init (&super, &path_element, vpath, mcerror, LIBSSH2_SFTP_LSTAT, &attrs);
    if (res < 0)
        return res;

    attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = mode;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res =
            libssh2_sftp_stat_ex (super->sftp_session, fixfname->str, fixfname->len,
                                  LIBSSH2_SFTP_SETSTAT, &attrs);
        if (res >= 0)
            break;

        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_NO_SUCH_FILE))
            return -ENOENT;

        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_FAILURE))
        {
            res = 0;            /* need something like ftpfs_ignore_chattr_errors */
            break;
        }

        if (!sftpfs_waitsocket (super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Delete a name from the file system.
 *
 * @param vpath   path to file or directory
 * @param mcerror pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_unlink (const vfs_path_t * vpath, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (&super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    do
    {
        res = libssh2_sftp_unlink_ex (super->sftp_session, fixfname->str, fixfname->len);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (super, res, mcerror))
            return -1;
    }
    while (res == LIBSSH2_ERROR_EAGAIN);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Rename a file, moving it between directories if required.
 *
 * @param vpath1   path to source file or directory
 * @param vpath2   path to destination file or directory
 * @param mcerror  pointer to error object
 * @return 0 if success, negative value otherwise
 */

int
sftpfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2, GError ** mcerror)
{
    sftpfs_super_t *super = NULL;
    const char *path1;
    const vfs_path_element_t *path_element2 = NULL;
    const GString *ctmp_path;
    char *tmp_path;
    unsigned int tmp_path_len;
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (&super, &path_element2, vpath2, mcerror))
        return -1;

    ctmp_path = sftpfs_fix_filename (path_element2->path);
    tmp_path = g_strndup (ctmp_path->str, ctmp_path->len);
    tmp_path_len = ctmp_path->len;

    path1 = vfs_path_get_last_path_str (vpath1);
    fixfname = sftpfs_fix_filename (path1);

    do
    {
        res =
            libssh2_sftp_rename_ex (super->sftp_session, fixfname->str, fixfname->len, tmp_path,
                                    tmp_path_len, LIBSSH2_SFTP_SYMLINK);
        if (res >= 0)
            break;

        if (!sftpfs_waitsocket (super, res, mcerror))
        {
            g_free (tmp_path);
            return -1;
        }
    }
    while (res == LIBSSH2_ERROR_EAGAIN);
    g_free (tmp_path);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
