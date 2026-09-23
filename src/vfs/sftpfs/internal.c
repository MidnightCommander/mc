/* Virtual File System: SFTP file system.
   The internal functions

   Copyright (C) 2011-2026
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#include "lib/vfs/utilvfs.h"

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
    s->st_blksize = LIBSSH2_CHANNEL_WINDOW_DEFAULT;  // FIXME
#endif
    vfs_adjust_stat (s);
}

static const char *
sftpfs_sftp_errstr (unsigned long sftp_error)
{
    static const char *sftp_error_messages[] = {
        /* LIBSSH2_FX_OK */ N_ ("OK"),
        /* LIBSSH2_FX_EOF */ N_ ("EOF"),
        /* LIBSSH2_FX_NO_SUCH_FILE */ N_ ("No such file"),
        /* LIBSSH2_FX_PERMISSION_DENIED */ N_ ("Permission denied"),
        /* LIBSSH2_FX_FAILURE */ N_ ("Failure"),
        /* LIBSSH2_FX_BAD_MESSAGE */ N_ ("Bad message"),
        /* LIBSSH2_FX_NO_CONNECTION */ N_ ("No connection"),
        /* LIBSSH2_FX_CONNECTION_LOST */ N_ ("Connection lost"),
        /* LIBSSH2_FX_OP_UNSUPPORTED */ N_ ("Operation unsupported"),
        /* LIBSSH2_FX_INVALID_HANDLE */ N_ ("Invalid handle"),
        /* LIBSSH2_FX_NO_SUCH_PATH */ N_ ("No such path"),
        /* LIBSSH2_FX_FILE_ALREADY_EXISTS */ N_ ("File already exists"),
        /* LIBSSH2_FX_WRITE_PROTECT */ N_ ("Write protect"),
        /* LIBSSH2_FX_NO_MEDIA */ N_ ("No media"),
        /* LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM */ N_ ("No space on filesystem"),
        /* LIBSSH2_FX_QUOTA_EXCEEDED */ N_ ("Quota exceeded"),
        /* LIBSSH2_FX_UNKNOWN_PRINCIPAL */ N_ ("Unknown principal"),
        /* LIBSSH2_FX_LOCK_CONFLICT */ N_ ("Lock conflict"),
        /* LIBSSH2_FX_DIR_NOT_EMPTY */ N_ ("Directory not empty"),
        /* LIBSSH2_FX_NOT_A_DIRECTORY */ N_ ("Not a directory"),
        /* LIBSSH2_FX_INVALID_FILENAME */ N_ ("Invalid filename"),
        /* LIBSSH2_FX_LINK_LOOP */ N_ ("Link loop"),
    };
    static char buffer[64];
    const char *sftp_errstr = (sftp_error < G_N_ELEMENTS (sftp_error_messages))
        ? sftp_error_messages[sftp_error]
        : buffer;

    if (sftp_errstr == buffer)
        g_snprintf (buffer, sizeof (buffer), N_ ("Unknown error code %lu"), sftp_error);
    return sftp_errstr;
}

static int
sftpfs_sftp_error_to_errno (unsigned long sftp_error)
{
    // Map libssh2's extended SFTP error codes to POSIX errnos.
    // When there's no exact match, map to E_REMOTE.
    static int sftp_errnos[] = {
        /* LIBSSH2_FX_OK */ 0,
        /* LIBSSH2_FX_EOF */ E_REMOTE,
        /* LIBSSH2_FX_NO_SUCH_FILE */ ENOENT,
        /* LIBSSH2_FX_PERMISSION_DENIED */ EACCES,
        /* LIBSSH2_FX_FAILURE */ E_REMOTE,
        /* LIBSSH2_FX_BAD_MESSAGE */ EBADMSG,
        /* LIBSSH2_FX_NO_CONNECTION */ ENOTCONN,
        /* LIBSSH2_FX_CONNECTION_LOST */ E_REMOTE,
        /* LIBSSH2_FX_OP_UNSUPPORTED */ EOPNOTSUPP,
        /* LIBSSH2_FX_INVALID_HANDLE */ EBADF,
        /* LIBSSH2_FX_NO_SUCH_PATH */ ENOENT,
        /* LIBSSH2_FX_FILE_ALREADY_EXISTS */ EEXIST,
        /* LIBSSH2_FX_WRITE_PROTECT */ EROFS,
#ifdef ENOMEDIUM
        /* LIBSSH2_FX_NO_MEDIA */ ENOMEDIUM,
#else
        /* LIBSSH2_FX_NO_MEDIA */ E_REMOTE,
#endif
        /* LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM */ ENOSPC,
        /* LIBSSH2_FX_QUOTA_EXCEEDED */ EDQUOT,
        /* LIBSSH2_FX_UNKNOWN_PRINCIPAL */ E_REMOTE,
        /* LIBSSH2_FX_LOCK_CONFLICT */ EDEADLK,
        /* LIBSSH2_FX_DIR_NOT_EMPTY */ ENOTEMPTY,
        /* LIBSSH2_FX_NOT_A_DIRECTORY */ ENOTDIR,
        /* LIBSSH2_FX_INVALID_FILENAME */ E_REMOTE,
        /* LIBSSH2_FX_LINK_LOOP */ ELOOP,
    };

    return (sftp_error < G_N_ELEMENTS (sftp_errnos)) ? sftp_errnos[sftp_error] : E_REMOTE;
}

static int
sftpfs_libssh2_error_to_errno (int code)
{
    // Map libssh2's error codes to POSIX errnos.
    // When there's no exact match, map to E_REMOTE.
    static int libssh2_errnos[] = {
        /* LIBSSH2_ERROR_NONE */ 0,
        /* LIBSSH2_ERROR_SOCKET_NONE */ E_REMOTE,
        /* LIBSSH2_ERROR_BANNER_RECV */ E_REMOTE,
        /* LIBSSH2_ERROR_BANNER_SEND */ E_REMOTE,
        /* LIBSSH2_ERROR_INVALID_MAC */ E_REMOTE,
        /* LIBSSH2_ERROR_KEX_FAILURE */ E_REMOTE,
        /* LIBSSH2_ERROR_ALLOC */ ENOMEM,
#ifdef ECOMM
        /* LIBSSH2_ERROR_SOCKET_SEND */ ECOMM,
#else
        /* LIBSSH2_ERROR_SOCKET_SEND */ E_REMOTE,
#endif
        /* LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE */ E_REMOTE,
        /* LIBSSH2_ERROR_TIMEOUT */ ETIMEDOUT,
        /* LIBSSH2_ERROR_HOSTKEY_INIT */ E_REMOTE,
        /* LIBSSH2_ERROR_HOSTKEY_SIGN */ E_REMOTE,
        /* LIBSSH2_ERROR_DECRYPT */ E_REMOTE,
        /* LIBSSH2_ERROR_SOCKET_DISCONNECT */ E_REMOTE,
        /* LIBSSH2_ERROR_PROTO */ EPROTO,
        /* LIBSSH2_ERROR_PASSWORD_EXPIRED */ E_REMOTE,
        /* LIBSSH2_ERROR_FILE */ E_REMOTE,
        /* LIBSSH2_ERROR_METHOD_NONE */ E_REMOTE,
        /* LIBSSH2_ERROR_AUTHENTICATION_FAILED */ E_REMOTE,
        /* LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_OUTOFORDER */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_FAILURE */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_UNKNOWN */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_CLOSED */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_EOF_SENT */ E_REMOTE,
        /* LIBSSH2_ERROR_SCP_PROTOCOL */ E_REMOTE,
        /* LIBSSH2_ERROR_ZLIB */ E_REMOTE,
        /* LIBSSH2_ERROR_SOCKET_TIMEOUT */ E_REMOTE,
        /* LIBSSH2_ERROR_SFTP_PROTOCOL */ E_REMOTE,
        /* LIBSSH2_ERROR_REQUEST_DENIED */ E_REMOTE,
        /* LIBSSH2_ERROR_METHOD_NOT_SUPPORTED */ E_REMOTE,
        /* LIBSSH2_ERROR_INVAL */ EINVAL,
        /* LIBSSH2_ERROR_INVALID_POLL_TYPE */ E_REMOTE,
        /* LIBSSH2_ERROR_PUBLICKEY_PROTOCOL */ E_REMOTE,
        /* LIBSSH2_ERROR_EAGAIN */ EAGAIN,
        /* LIBSSH2_ERROR_BUFFER_TOO_SMALL */ E_REMOTE,
        /* LIBSSH2_ERROR_BAD_USE */ E_REMOTE,
        /* LIBSSH2_ERROR_COMPRESS */ E_REMOTE,
        /* LIBSSH2_ERROR_OUT_OF_BOUNDARY */ E_REMOTE,
        /* LIBSSH2_ERROR_AGENT_PROTOCOL */ E_REMOTE,
        /* LIBSSH2_ERROR_SOCKET_RECV */ E_REMOTE,
        /* LIBSSH2_ERROR_ENCRYPT */ E_REMOTE,
        /* LIBSSH2_ERROR_BAD_SOCKET */ EBADF,
        /* LIBSSH2_ERROR_KNOWN_HOSTS */ E_REMOTE,
        /* LIBSSH2_ERROR_CHANNEL_WINDOW_FULL */ E_REMOTE,
        /* LIBSSH2_ERROR_KEYFILE_AUTH_FAILED */ E_REMOTE,
        /* LIBSSH2_ERROR_RANDGEN */ E_REMOTE,
        /* LIBSSH2_ERROR_MISSING_USERAUTH_BANNER */ E_REMOTE,
        /* LIBSSH2_ERROR_ALGO_UNSUPPORTED */ EOPNOTSUPP,
        /* LIBSSH2_ERROR_MAC_FAILURE */ E_REMOTE,
        /* LIBSSH2_ERROR_HASH_INIT */ E_REMOTE,
        /* LIBSSH2_ERROR_HASH_CALC */ E_REMOTE,
    };

    g_return_val_if_fail (code <= 0, 0);

    return ((unsigned) -code < G_N_ELEMENTS (libssh2_errnos)) ? libssh2_errnos[-code] : E_REMOTE;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_stat_init (sftpfs_super_t **super, const vfs_path_element_t **path_element,
                  const vfs_path_t *vpath, GError **mcerror, int stat_type,
                  LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (super, path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename ((*path_element)->path);

    res = libssh2_sftp_stat_ex ((*super)->sftp_session, fixfname->str, fixfname->len, stat_type,
                                attrs);
    if (res < 0)
    {
        sftpfs_ssherror_to_gliberror (*super, res, mcerror);
        return -1;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
sftpfs_is_sftp_error (LIBSSH2_SFTP *sftp_session, int sftp_res, int sftp_error)
{
    return (sftp_res == LIBSSH2_ERROR_SFTP_PROTOCOL
            && libssh2_sftp_last_error (sftp_session) == (unsigned long) sftp_error);
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
sftpfs_ssherror_to_gliberror (sftpfs_super_t *super, int libssh_errno, GError **mcerror)
{
    int converted_errno;

    mc_return_if_error (mcerror);

    if (libssh_errno == LIBSSH2_ERROR_SFTP_PROTOCOL && super->sftp_session != NULL)
    {
        unsigned long sftp_error = libssh2_sftp_last_error (super->sftp_session);
        converted_errno = sftpfs_sftp_error_to_errno (sftp_error);
        mc_propagate_error (mcerror, converted_errno, "SFTP: %s",
                            _ (sftpfs_sftp_errstr (sftp_error)));
    }
    else
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (super->session, &err, &err_len, 1);
        converted_errno = sftpfs_libssh2_error_to_errno (libssh_errno);
        mc_propagate_error (mcerror, converted_errno, "%s", err);
        g_free (err);
    }
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
sftpfs_op_init (sftpfs_super_t **super, const vfs_path_element_t **path_element,
                const vfs_path_t *vpath, GError **mcerror)
{
    mc_return_val_if_error (mcerror, FALSE);

    do
    {
        struct vfs_s_super *lc_super = NULL;

        if (vfs_s_get_path (vpath, &lc_super, 0) == NULL)
            break;

        if (lc_super == NULL)
            break;

        *super = SFTP_SUPER (lc_super);
        if ((*super)->sftp_session == NULL)
            break;

        *path_element = vfs_path_get_by_index (vpath, -1);

        return TRUE;
    }
    while (0);

    mc_propagate_error (mcerror, ENOENT, _ ("sftp: %s"), unix_error_string (ENOENT));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
sftpfs_attr_to_stat (const LIBSSH2_SFTP_ATTRIBUTES *attrs, struct stat *s)
{
    if ((attrs->flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        s->st_uid = attrs->uid;
        s->st_gid = attrs->gid;
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        vfs_zero_stat_times (s);
        s->st_atime = attrs->atime;
        s->st_mtime = attrs->mtime;
        s->st_ctime = attrs->mtime;
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
sftpfs_lstat (const vfs_path_t *vpath, struct stat *buf, GError **mcerror)
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
sftpfs_stat (const vfs_path_t *vpath, struct stat *buf, GError **mcerror)
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
 * @return        bytes written without NUL, negative value on failure
 */

int
sftpfs_readlink (const vfs_path_t *vpath, char *buf, size_t size, GError **mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (&super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    res = libssh2_sftp_symlink_ex (super->sftp_session, fixfname->str, fixfname->len, buf, size,
                                   LIBSSH2_SFTP_READLINK);
    if (res < 0)
    {
        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

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
sftpfs_symlink (const vfs_path_t *vpath1, const vfs_path_t *vpath2, GError **mcerror)
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

    res = libssh2_sftp_symlink_ex (super->sftp_session, path1, path1_len, tmp_path, tmp_path_len,
                                   LIBSSH2_SFTP_SYMLINK);
    g_free (tmp_path);

    if (res < 0)
    {
        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

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
sftpfs_utime (const vfs_path_t *vpath, time_t atime, time_t mtime, GError **mcerror)
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

    res = libssh2_sftp_stat_ex (super->sftp_session, fixfname->str, fixfname->len,
                                LIBSSH2_SFTP_SETSTAT, &attrs);
    if (res < 0)
    {
        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_FAILURE))
            return 0;  // need something like ftpfs_ignore_chattr_errors

        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

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
sftpfs_chmod (const vfs_path_t *vpath, mode_t mode, GError **mcerror)
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

    res = libssh2_sftp_stat_ex (super->sftp_session, fixfname->str, fixfname->len,
                                LIBSSH2_SFTP_SETSTAT, &attrs);
    if (res < 0)
    {
        if (sftpfs_is_sftp_error (super->sftp_session, res, LIBSSH2_FX_FAILURE))
            return 0;  // need something like ftpfs_ignore_chattr_errors

        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

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
sftpfs_unlink (const vfs_path_t *vpath, GError **mcerror)
{
    sftpfs_super_t *super = NULL;
    const vfs_path_element_t *path_element = NULL;
    const GString *fixfname;
    int res;

    if (!sftpfs_op_init (&super, &path_element, vpath, mcerror))
        return -1;

    fixfname = sftpfs_fix_filename (path_element->path);

    res = libssh2_sftp_unlink_ex (super->sftp_session, fixfname->str, fixfname->len);
    if (res < 0)
    {
        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

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
sftpfs_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2, GError **mcerror)
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

    res = libssh2_sftp_rename_ex (super->sftp_session, fixfname->str, fixfname->len, tmp_path,
                                  tmp_path_len, LIBSSH2_SFTP_SYMLINK);
    g_free (tmp_path);
    if (res < 0)
    {
        sftpfs_ssherror_to_gliberror (super, res, mcerror);
        return -1;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
