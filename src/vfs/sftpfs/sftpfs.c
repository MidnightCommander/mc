/* Virtual File System: SFTP file system.

   Copyright (C)
   2011 Free Software Foundation, Inc.

   Authors:
   2011 Maslakov Ilia
   2011 Slava Zanko

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */


/**
 * \file
 * \brief Source: sftpfs FS
 */

#include <config.h>
#include <errno.h>
#include <sys/types.h>

#include <netdb.h>              /* struct hostent */
#include <sys/socket.h>         /* AF_INET */
#include <netinet/in.h>         /* struct in_addr */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"

#include "lib/util.h"
#include "lib/tty/tty.h"        /* tty_enable_interrupt_key () */
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"
#include "lib/vfs/netutil.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_stamp_create */
#include "lib/event.h"

#include "sftpfs.h"
#include "dialogs.h"

/*** global variables ****************************************************************************/

int sftpfs_timeout = 0;
char *sftpfs_privkey = NULL;
char *sftpfs_pubkey = NULL;
char *sftpfs_user = NULL;
char *sftpfs_host = NULL;
int sftpfs_port = 22;
int sftpfs_auth_method;
gboolean sftpfs_newcon = TRUE;

/*** file scope macro definitions ****************************************************************/

#define SUP ((sftpfs_super_data_t *) super->data)

#define SFTP_ESTABLISHED     1
#define SFTP_FAILED        500

#define SFTP_DEFAULT_PORT 22

#define SFTP_HANDLE_MAXLEN 256  /* according to spec! */

#define SFTP_AUTH_AUTO     -1
#define SFTP_AUTH_PASSWORD  0
#define SFTP_AUTH_PUBLICKEY 1
#define SFTP_AUTH_SSHAGENT  2

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int auth_pw;

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;

    int socket_handle;
    const char *fingerprint;
} sftpfs_super_data_t;

typedef struct
{
    LIBSSH2_SFTP_HANDLE *handle;
    int flags;
    mode_t mode;
} sftpfs_fh_data_t;

typedef struct
{
    LIBSSH2_SFTP_HANDLE *handle;
} sftpfs_dir_data_t;

/*** file scope variables ************************************************************************/

static struct vfs_class vfs_sftpfs_ops;
static const char *vfs_my_name = "sftpfs";

static int sftpfs_errno_int;

GString *sftpfs_string_buffer = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_archive_same (const vfs_path_element_t * vpath_element, struct vfs_s_super *super,
                     const vfs_path_t * vpath, void *cookie)
{
    vfs_path_element_t *path_element;
    int result;

    (void) vpath;
    (void) cookie;

    path_element = vfs_path_element_clone (vpath_element);

    if (path_element->user == NULL)
        path_element->user = vfs_get_local_username ();

    if (path_element->port == 0)
        path_element->port = SFTP_DEFAULT_PORT;

    result = ((strcmp (path_element->host, super->path_element->host) == 0)
              && (strcmp (path_element->user, super->path_element->user) == 0)
              && (path_element->port == super->path_element->port)) ? 1 : 0;

    vfs_path_element_free (path_element);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_waitsocket (int socket_fd, LIBSSH2_SESSION * session)
{
    struct timeval timeout = { 10, 0 };
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    FD_ZERO (&fd);
    FD_SET (socket_fd, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions (session);

    if ((dir & LIBSSH2_SESSION_BLOCK_INBOUND) != 0)
        readfd = &fd;

    if ((dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0)
        writefd = &fd;

    return select (socket_fd + 1, readfd, writefd, NULL, &timeout);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_fh_open (struct vfs_class *me, vfs_file_handler_t * fh, int flags, mode_t mode)
{
    unsigned long sftp_open_flags = 0;
    sftpfs_fh_data_t *sftpfs_fh;
    sftpfs_super_data_t *sftpfs_super_data = (sftpfs_super_data_t *) FH_SUPER->data;
    int sftp_open_mode = 0;
    char *name;

    (void) mode;

    fh->data = sftpfs_fh = g_new0 (sftpfs_fh_data_t, 1);

    if ((flags & O_CREAT) != 0 || (flags & O_WRONLY) != 0)
    {
        sftp_open_flags = ((flags & O_WRONLY) != 0 ? LIBSSH2_FXF_WRITE : 0) |
            ((flags & O_CREAT) != 0 ? LIBSSH2_FXF_CREAT : 0) |
            ((flags & ~O_APPEND) != 0 ? LIBSSH2_FXF_TRUNC : 0);
        sftp_open_mode = LIBSSH2_SFTP_S_IRUSR |
            LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH;

    }
    else
        sftp_open_flags = LIBSSH2_FXF_READ;

    name = vfs_s_fullpath (me, fh->ino);
    if (name == NULL)
    {
        g_free (fh->data);
        return -1;
    }
    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", name);
    g_free (name);

    sftpfs_fh->handle =
        libssh2_sftp_open (sftpfs_super_data->sftp_session, sftpfs_string_buffer->str,
                           sftp_open_flags, sftp_open_mode);

    if (sftpfs_fh->handle == NULL)
    {
        g_free (fh->data);
        return -1;
    }

    sftpfs_fh->flags = flags;
    sftpfs_fh->mode = mode;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_fh_close (struct vfs_class *me, vfs_file_handler_t * fh)
{
    sftpfs_fh_data_t *sftpfs_fh = (sftpfs_fh_data_t *) fh->data;

    (void) me;

    if (sftpfs_fh->handle == NULL)
        return -1;

    libssh2_sftp_close (sftpfs_fh->handle);

    g_free (fh->data);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void *
sftpfs_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    int was_changed = 0;
    vfs_file_handler_t *fh;
    struct vfs_s_super *super;
    const char *q;
    struct vfs_s_inode *ino;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (strcmp (path_element->host, sftpfs_host) != 0)
    {
        g_free (path_element->host);
        path_element->host = g_strdup (sftpfs_host);
    }

    q = vfs_s_get_path (vpath, &super, 0);
    if (q == NULL)
        return NULL;

    ino = vfs_s_find_inode (path_element->class, super, q, LINK_FOLLOW, FL_NONE);
    if (ino != NULL && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
    {
        path_element->class->verrno = EEXIST;
        return NULL;
    }

    if (ino == NULL)
    {
        char *dirname, *name;
        struct vfs_s_entry *ent;
        struct vfs_s_inode *dir;

        dirname = g_path_get_dirname (q);
        name = g_path_get_basename (q);
        dir = vfs_s_find_inode (path_element->class, super, dirname, LINK_FOLLOW, FL_DIR);
        if (dir == NULL)
        {
            g_free (dirname);
            g_free (name);
            return NULL;
        }
        ent = vfs_s_generate_entry (path_element->class, name, dir, 0755);
        ino = ent->ino;
        vfs_s_insert_entry (path_element->class, dir, ent);
        g_free (dirname);
        g_free (name);
        was_changed = 1;
    }

    if (S_ISDIR (ino->st.st_mode))
    {
        path_element->class->verrno = EISDIR;
        return NULL;
    }

    fh = g_new (vfs_file_handler_t, 1);
    fh->pos = 0;
    fh->ino = ino;
    fh->handle = -1;
    fh->changed = was_changed;
    fh->linear = 0;
    fh->data = NULL;

    if (sftpfs_fh_open (path_element->class, fh, flags, mode) != 0)
    {
        g_free (fh);
        return NULL;
    }

    vfs_rmstamp (path_element->class, (vfsid) super);
    super->fd_usage++;
    fh->ino->st.st_nlink++;
    return fh;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_close (void *fh)
{
    int res = 0;
    struct vfs_class *me = FH_SUPER->me;

    FH_SUPER->fd_usage--;
    if (FH_SUPER->fd_usage == 0)
        vfs_stamp_create (me, FH_SUPER);

    res = sftpfs_fh_close (me, fh);

    if (FH->handle != -1)
        close (FH->handle);

    vfs_s_free_inode (me, FH->ino);
    g_free (fh);
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void *
sftpfs_opendir (const vfs_path_t * vpath)
{
    sftpfs_dir_data_t *sftpfs_dir;
    struct vfs_s_super *super;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    /* reset interrupt flag */
    tty_got_interrupt ();

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return NULL;

    sftpfs_dir = g_new0 (sftpfs_dir_data_t, 1);
    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    sftpfs_dir->handle = libssh2_sftp_opendir (SUP->sftp_session, sftpfs_string_buffer->str);

    if (sftpfs_dir->handle == NULL)
    {
        if (libssh2_session_last_errno (SUP->session) != LIBSSH2_ERROR_EAGAIN)
        {
            g_free (sftpfs_dir);
            return NULL;
        }

        sftpfs_waitsocket (SUP->socket_handle, SUP->session);
    }

    if (sftpfs_dir->handle == NULL)
    {
        g_free (sftpfs_dir);
        sftpfs_dir = NULL;
    }

    return (void *) sftpfs_dir;
}

/* --------------------------------------------------------------------------------------------- */

static void *
sftpfs_readdir (void *data)
{
    char mem[BUF_MEDIUM];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;

    static union vfs_dirent sftpfs_dirent;

    int rc;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return NULL;
    }

    rc = libssh2_sftp_readdir (sftpfs_dir->handle, mem, sizeof (mem), &attrs);

    /* rc is the length of the file name in the mem buffer */
    if (rc <= 0)
    {
        vfs_print_message (_("sftp: Listing done."));
        return NULL;
    }

    if (mem[0] != '\0')
        vfs_print_message (_("sftp: (Ctrl-G break) Listing... %s"), mem);

    g_strlcpy (sftpfs_dirent.dent.d_name, mem, BUF_MEDIUM);
    compute_namelen (&sftpfs_dirent.dent);
    return &sftpfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_closedir (void *data)
{
    int ret;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;

    ret = libssh2_sftp_closedir (sftpfs_dir->handle);
    g_free (sftpfs_dir);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
static int
sftpfs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
{
    (void) me;
    (void) dir;
    (void) remote_path;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_lstat (const vfs_path_t * vpath, struct stat *buf)
{
    struct vfs_s_super *super;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL || SUP->sftp_session == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    res = libssh2_sftp_stat_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                                sftpfs_string_buffer->len, LIBSSH2_SFTP_LSTAT, &attrs);

    if (res < 0)
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (SUP->session, &err, &err_len, 1);
        g_free (err);
        return -1;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_stat (const vfs_path_t * vpath, struct stat *buf)
{
    struct vfs_s_super *super;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL || SUP->sftp_session == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    res = libssh2_sftp_stat_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                                sftpfs_string_buffer->len, LIBSSH2_SFTP_STAT, &attrs);

    if (res < 0)
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (SUP->session, &err, &err_len, 1);
        g_free (err);
        return -1;
    }

    buf->st_nlink = 1;
    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_fstat (void *data, struct stat *buf)
{
    int res;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;
    sftpfs_fh_data_t *sftpfs_fh = fh->data;
    struct vfs_s_super *super = fh->ino->super;

    if (sftpfs_fh->handle == NULL)
        return -1;

    do
    {
        res = libssh2_sftp_fstat_ex (sftpfs_fh->handle, &attrs, 0);

        if (res < 0)
        {
            if (libssh2_session_last_errno (SUP->session) != LIBSSH2_ERROR_EAGAIN)
                return -1;
            sftpfs_waitsocket (SUP->socket_handle, SUP->session);
        }
    }
    while (res < 0);

    if (res < 0)
        return -1;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_chmod (const vfs_path_t * vpath, mode_t mode)
{
    struct vfs_s_super *super;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL || SUP->sftp_session == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);

    res = libssh2_sftp_stat_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                                sftpfs_string_buffer->len, LIBSSH2_SFTP_LSTAT, &attrs);

    if (res < 0)
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (SUP->session, &err, &err_len, 1);
        g_free (err);
        return -1;
    }

    attrs.permissions = mode;

    return libssh2_sftp_stat_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                                sftpfs_string_buffer->len, LIBSSH2_SFTP_SETSTAT, &attrs);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
{
    (void) vpath;
    (void) owner;
    (void) group;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_utime (const vfs_path_t * vpath, struct utimbuf *times)
{
    (void) vpath;
    (void) times;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_readlink (const vfs_path_t * vpath, char *buf, size_t size)
{
    struct vfs_s_super *super;
    int res;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    do
    {
        g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);

        res = libssh2_sftp_readlink (SUP->sftp_session, sftpfs_string_buffer->str, buf, size);

        if (res < 0)
        {
            if (libssh2_session_last_errno (SUP->session) != LIBSSH2_ERROR_EAGAIN)
                return -1;

            sftpfs_waitsocket (SUP->socket_handle, SUP->session);
        }
    }
    while (res < 0);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_unlink (const vfs_path_t * vpath)
{
    struct vfs_s_super *super;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    libssh2_sftp_unlink_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                            sftpfs_string_buffer->len);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    struct vfs_s_super *super;
    vfs_path_element_t *path_element1;
    vfs_path_element_t *path_element2;
    char *tmp_path;

    path_element2 = vfs_path_get_by_index (vpath2, -1);

    if (vfs_s_get_path (vpath2, &super, 0) == NULL)
        return -1;

    tmp_path = mc_build_filename (PATH_SEP_STR, path_element2->path, NULL);
    path_element1 = vfs_path_get_by_index (vpath1, -1);
    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element1->path);

    libssh2_sftp_symlink_ex (SUP->sftp_session, sftpfs_string_buffer->str,
                             sftpfs_string_buffer->len, tmp_path, strlen (tmp_path),
                             LIBSSH2_SFTP_SYMLINK);

    g_free (tmp_path);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
sftpfs_write (void *data, const char *buf, size_t nbyte)
{
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;
    sftpfs_fh_data_t *sftpfs_fh = fh->data;

    fh->pos = (off_t) libssh2_sftp_tell64 (sftpfs_fh->handle);

    return libssh2_sftp_write (sftpfs_fh->handle, buf, nbyte);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    struct vfs_s_super *super;
    vfs_path_element_t *path_element1;
    vfs_path_element_t *path_element2;
    char *tmp_path;

    path_element2 = vfs_path_get_by_index (vpath2, -1);

    if (vfs_s_get_path (vpath2, &super, 0) == NULL)
        return -1;

    tmp_path = mc_build_filename (PATH_SEP_STR, path_element2->path, NULL);
    path_element1 = vfs_path_get_by_index (vpath1, -1);
    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element1->path);

    libssh2_sftp_rename_ex (SUP->sftp_session, sftpfs_string_buffer->str, sftpfs_string_buffer->len,
                            tmp_path, strlen (tmp_path), 0);
    g_free (tmp_path);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_mknod (const vfs_path_t * vpath, mode_t mode, dev_t dev)
{
    (void) vpath;
    (void) mode;
    (void) dev;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_link (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    (void) vpath1;
    (void) vpath2;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    struct vfs_s_super *super;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    if (libssh2_sftp_mkdir (SUP->sftp_session, sftpfs_string_buffer->str, mode) < 0)
        return -1;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_rmdir (const vfs_path_t * vpath)
{
    struct vfs_s_super *super;
    vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    g_string_printf (sftpfs_string_buffer, PATH_SEP_STR "%s", path_element->path);
    libssh2_sftp_rmdir_ex (SUP->sftp_session, sftpfs_string_buffer->str, sftpfs_string_buffer->len);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftpfs_plugin_name_for_config_dialog (const gchar * event_group_name, const gchar * event_name,
                                      gpointer init_data, gpointer data)
{
    GList **list = data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    *list = g_list_append (*list, (gpointer) vfs_my_name);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftpfs_plugin_show_config_dialog (const gchar * event_group_name, const gchar * event_name,
                                  gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    return ((const char *) data != vfs_my_name);
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
sftpfs_read (void *data, char *buffer, size_t count)
{
    int rc;

    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;
    sftpfs_fh_data_t *sftpfs_fh = fh->data;
    struct vfs_s_super *super = fh->ino->super;

    if (tty_got_interrupt ())
    {
        tty_disable_interrupt_key ();
        return 0;
    }

    do
    {
        rc = libssh2_sftp_read (sftpfs_fh->handle, buffer, count);
        if (rc < 0)
        {
            if (libssh2_session_last_errno (SUP->session) != LIBSSH2_ERROR_EAGAIN)
                return -1;

            sftpfs_waitsocket (SUP->socket_handle, SUP->session);
        }

    }
    while (rc < 0);

    fh->pos = (off_t) libssh2_sftp_tell64 (sftpfs_fh->handle);

    return (rc >= 0) ? rc : -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_errno (struct vfs_class *me)
{
    (void) me;
    /*
       struct vfs_s_super *super;
       super = sftpfs_get_super (me);
       if (super != NULL && SUP->session != NULL)
       return libssh2_session_last_errno (SUP->session);
     */
    return errno;
}

/* --------------------------------------------------------------------------------------------- */

static void
_sftpfs_reopen (vfs_file_handler_t *fh)
{
    sftpfs_fh_data_t *sftpfs_fh = fh->data;
    int flags = sftpfs_fh->flags;
    mode_t mode = sftpfs_fh->mode;
    struct vfs_class *me = fh->ino->super->me;

    sftpfs_fh_close (me, fh);
    sftpfs_fh_open (me, fh, flags, mode);
}

/* --------------------------------------------------------------------------------------------- */

static off_t
sftpfs_lseek (void *data, off_t offset, int whence)
{
    vfs_file_handler_t *fh = (vfs_file_handler_t *) data;
    sftpfs_fh_data_t *sftpfs_fh = fh->data;

    switch (whence)
    {
    case SEEK_SET:
        /* Need reopen file becous:
           "You MUST NOT seek during writing or reading a file with SFTP, as the internals use
           outstanding packets and changing the "file position" during transit will results in
           badness." */
        if (fh->pos > offset || offset == 0)
            _sftpfs_reopen (fh);
        fh->pos = offset;
        break;
    case SEEK_CUR:
        fh->pos += offset;
        break;
    case SEEK_END:
        if (fh->pos > fh->ino->st.st_size - offset)
            _sftpfs_reopen (fh);
        fh->pos = fh->ino->st.st_size - offset;
        break;
    }

    libssh2_sftp_seek64 (sftpfs_fh->handle, fh->pos);
    fh->pos = (off_t) libssh2_sftp_tell64 (sftpfs_fh->handle);

    return fh->pos;

}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_open_socket (struct vfs_s_super *super)
{
    struct addrinfo hints, *res, *curr_res;
    int my_socket = 0;
    char *host = NULL;
    char port[BUF_TINY];
    int e;

    if (super->path_element->host == NULL || *super->path_element->host == '\0')
    {
        vfs_print_message (_("sftp: Invalid host name."));
        g_free (host);
        return -1;
    }

    sprintf (port, "%hu", (unsigned short) super->path_element->port);
    if (port == NULL)
    {
        vfs_print_message (_("sftp: Invalid port value."));
        return -1;
    }

    tty_enable_interrupt_key ();        /* clear the interrupt flag */

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
    /* By default, only look up addresses using address types for
     * which a local interface is configured (i.e. no IPv6 if no IPv6
     * interfaces, likewise for IPv4 (see RFC 3493 for details). */
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    e = getaddrinfo (super->path_element->host, port, &hints, &res);

#ifdef AI_ADDRCONFIG
    if (e == EAI_BADFLAGS)
    {
        /* Retry with no flags if AI_ADDRCONFIG was rejected. */
        hints.ai_flags = 0;
        e = getaddrinfo (super->path_element->host, port, &hints, &res);
    }
#endif

    if (e != 0)
    {
        tty_disable_interrupt_key ();
        vfs_print_message (_("sftp: %s"), gai_strerror (e));
        return -1;
    }

    for (curr_res = res; curr_res != NULL; curr_res = curr_res->ai_next)
    {
        my_socket = socket (curr_res->ai_family, curr_res->ai_socktype, curr_res->ai_protocol);

        if (my_socket < 0)
        {
            if (curr_res->ai_next != NULL)
                continue;

            tty_disable_interrupt_key ();
            vfs_print_message (_("sftp: %s"), unix_error_string (errno));
            freeaddrinfo (res);
            sftpfs_errno_int = errno;
            return -1;
        }

        vfs_print_message (_("sftp: making connection to %s"), super->path_element->host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        sftpfs_errno_int = errno;
        close (my_socket);

        if (errno == EINTR && tty_got_interrupt ())
            vfs_print_message (_("sftp: connection interrupted by user"));
        else if (res->ai_next == NULL)
            vfs_print_message (_("sftp: connection to server failed: %s"),
                               unix_error_string (errno));
        else
            continue;

        freeaddrinfo (res);
        tty_disable_interrupt_key ();
        return -1;
    }

    freeaddrinfo (res);
    tty_disable_interrupt_key ();
    return my_socket;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_do_connect (struct vfs_class *me, struct vfs_s_super *super)
{
    int rc;
    char *userauthlist;

    (void) me;

    if (libssh2_init (0) != 0)
        return -1;

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */

    SUP->socket_handle = sftpfs_open_socket (super);

    /* Create a session instance */
    SUP->session = libssh2_session_init ();
    if (SUP->session == NULL)
        goto err_conn;

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */

    rc = libssh2_session_startup (SUP->session, SUP->socket_handle);
    if (rc != 0)
    {
        vfs_print_message (_("sftp: Failure establishing SSH session: (%d)"), rc);
        goto err_conn;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    SUP->fingerprint = libssh2_hostkey_hash (SUP->session, LIBSSH2_HOSTKEY_HASH_SHA1);
    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list (SUP->session, super->path_element->user,
                                          strlen (super->path_element->user));

    if (sftpfs_auth_method == SFTP_AUTH_AUTO)
    {
        if (strstr (userauthlist, "password") != NULL)
            SUP->auth_pw |= 1;
        if (strstr (userauthlist, "keyboard-interactive") != NULL)
            SUP->auth_pw |= 2;
        if (strstr(userauthlist, "publickey") != NULL)
            SUP->auth_pw |= 4;
    }
    else
    {
        SUP->auth_pw = 1;
        if (sftpfs_auth_method == SFTP_AUTH_PUBLICKEY)
            SUP->auth_pw = 4;
        if (sftpfs_auth_method == SFTP_AUTH_SSHAGENT)
            SUP->auth_pw = 8;

    }

    if (super->path_element->password == NULL && (SUP->auth_pw < 8))
    {
        char *p;

        if ((SUP->auth_pw & 4) != 0)
            p = g_strconcat (_("sftp: Enter passphrase for"), " ", super->path_element->user,
                             " ", NULL);
        else
            p = g_strconcat (_("sftp: Enter password for"), " ", super->path_element->user,
                             " ", NULL);
        super->path_element->password = vfs_get_password (p);
        g_free (p);

        if (super->path_element->password == NULL)
        {
            vfs_print_message (_("sftp: Password is empty."));
            goto err_conn;
        }
    }

    if ((SUP->auth_pw & 8) != 0)
    {
        LIBSSH2_AGENT *agent = NULL;
        struct libssh2_agent_publickey *identity, *prev_identity = NULL;

        /* Connect to the ssh-agent */
        agent = libssh2_agent_init (SUP->session);
        if (!agent)
        {
            vfs_print_message (_("sftp: Failure initializing ssh-agent support"));
            goto err_conn;
        }
        if (libssh2_agent_connect (agent))
        {
            vfs_print_message (_("sftp: Failure connecting to ssh-agent"));
            goto err_conn;
        }
        if (libssh2_agent_list_identities (agent))
        {
            vfs_print_message (_("sftp: Failure requesting identities to ssh-agent"));
            goto err_conn;
        }
        while (TRUE)
        {
            rc = libssh2_agent_get_identity (agent, &identity, prev_identity);
            if (rc == 1)
                break;
            if (rc < 0)
            {
                vfs_print_message (_("sftp: Failure obtaining identity from ssh-agent support"));
                goto err_conn;
            }
            if (libssh2_agent_userauth (agent, super->path_element->user, identity))
            {
                vfs_print_message (_("sftp: Authentication with public key %s failed"),
                    identity->comment);
            }
            else
                break;

            prev_identity = identity;
        }
    }
    else if ((SUP->auth_pw & 4) != 0)
    {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile (SUP->session, super->path_element->user,
                                                 sftpfs_pubkey, sftpfs_privkey,
                                                 super->path_element->password))
        {
            vfs_print_message (_("sftp: Authentication by public key failed"));
            goto err_conn;
        }

        vfs_print_message (_("sftp: Authentication by public key succeeded"));
    }
    else if ((SUP->auth_pw & 1) != 0)
    {
        /* We could authenticate via password */
        if (libssh2_userauth_password (SUP->session, super->path_element->user,
                                       super->path_element->password))
        {
            vfs_print_message (_("sftp: Authentication by password failed."));
            goto err_conn;
        }
    }
    else
    {
        vfs_print_message (_("sftp: No supported authentication methods found!"));
        goto err_conn;
    }

    SUP->sftp_session = libssh2_sftp_init (SUP->session);

    if (SUP->sftp_session == NULL)
        goto err_conn;
    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking (SUP->session, 1);

    return 0;

  err_conn:
    SUP->sftp_session = NULL;

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_open_archive (struct vfs_s_super *super,
                     const vfs_path_t * vpath, const vfs_path_element_t * vpath_element)
{
    char *sec = NULL;

    (void) vpath;

    if (vpath_element->host == NULL || *vpath_element->host == '\0')
    {
        vfs_print_message (_("sftp: Invalid host name."));
        vpath_element->class->verrno = EPERM;
        return 0;
    }

    super->data = g_new0 (sftpfs_super_data_t, 1);
    super->path_element = vfs_path_element_clone (vpath_element);

    if (super->path_element->user == NULL)
    {
        super->path_element->user = vfs_get_local_username ();
        if (super->path_element->user == NULL)
        {
            vfs_path_element_free (super->path_element);
            super->path_element = NULL;
            g_free (super->data);
            vpath_element->class->verrno = EPERM;
            return 0;
        }
    }

    sec = g_strdup_printf ("%s://%s@%s", vfs_sftpfs_ops.prefix,
                           super->path_element->user, super->path_element->host);

    sftpfs_load_param (sec);

    if (sftpfs_newcon)
    {
        sftpfs_user = g_strdup (super->path_element->user);
        sftpfs_host = g_strdup (super->path_element->host);
        if (super->path_element->port > 0)
            sftpfs_port = super->path_element->port;

        configure_sftpfs_conn (sec);
    }

    g_free (sec);
    g_free (super->path_element->user);
    g_free (super->path_element->host);

    g_free (vpath_element->host);
    ((vfs_path_element_t *) vpath_element)->host = g_strdup (sftpfs_host);

    g_free (vpath_element->user);
    ((vfs_path_element_t *) vpath_element)->user = g_strdup (sftpfs_user);

    ((vfs_path_element_t *) vpath_element)->port = sftpfs_port;

    super->path_element->user = g_strdup (sftpfs_user);
    super->path_element->host = g_strdup (sftpfs_host);
    super->path_element->port = sftpfs_port ;

    if (super->path_element->port == 0)
        super->path_element->port = SFTP_DEFAULT_PORT;

    SUP->auth_pw = 1;
    super->name = g_strdup (PATH_SEP_STR);
    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    return sftpfs_do_connect (vpath_element->class, super);
}

/* --------------------------------------------------------------------------------------------- */

static void
sftpfs_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    (void) me;

    if (SUP->sftp_session != NULL)
        libssh2_sftp_shutdown (SUP->sftp_session);

    if (SUP->session != NULL)
    {
        libssh2_session_disconnect (SUP->session, "Normal Shutdown");
        libssh2_session_free (SUP->session);
    }

    close (SUP->socket_handle);
    libssh2_exit ();

    g_free (super->data);
}

/* --------------------------------------------------------------------------------------------- */

static void
sftpfs_done (struct vfs_class *me)
{
    (void) me;

    g_free (sftpfs_privkey);
    g_free (sftpfs_pubkey);
    g_free (sftpfs_user);
    g_free (sftpfs_host);
    g_string_free (sftpfs_string_buffer, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_init (struct vfs_class *me)
{
    (void) me;

    sftpfs_string_buffer = g_string_new ("");

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

void
init_sftpfs (void)
{
    static struct vfs_s_subclass sftpfs_subclass;

    tcp_init ();

    sftpfs_subclass.flags = VFS_S_REMOTE;
    sftpfs_subclass.open_archive = sftpfs_open_archive;
    sftpfs_subclass.free_archive = sftpfs_free_archive;
    sftpfs_subclass.archive_same = sftpfs_archive_same;
    sftpfs_subclass.dir_load = sftpfs_dir_load;

    vfs_s_init_class (&vfs_sftpfs_ops, &sftpfs_subclass);

    vfs_sftpfs_ops.name = vfs_my_name;
    vfs_sftpfs_ops.prefix = "sftp";
    vfs_sftpfs_ops.flags = VFSF_NOLINKS;
    vfs_sftpfs_ops.init = sftpfs_init;
    vfs_sftpfs_ops.done = sftpfs_done;
    vfs_sftpfs_ops.read = sftpfs_read;
    vfs_sftpfs_ops.write = sftpfs_write;
    vfs_sftpfs_ops.open = sftpfs_open;
    vfs_sftpfs_ops.close = sftpfs_close;
    vfs_sftpfs_ops.opendir = sftpfs_opendir;
    vfs_sftpfs_ops.readdir = sftpfs_readdir;
    vfs_sftpfs_ops.closedir = sftpfs_closedir;
    vfs_sftpfs_ops.stat = sftpfs_stat;
    vfs_sftpfs_ops.lstat = sftpfs_lstat;
    vfs_sftpfs_ops.fstat = sftpfs_fstat;
    vfs_sftpfs_ops.chmod = sftpfs_chmod;
    vfs_sftpfs_ops.chown = sftpfs_chown;
    vfs_sftpfs_ops.utime = sftpfs_utime;
    vfs_sftpfs_ops.readlink = sftpfs_readlink;
    vfs_sftpfs_ops.symlink = sftpfs_symlink;
    vfs_sftpfs_ops.link = sftpfs_link;
    vfs_sftpfs_ops.unlink = sftpfs_unlink;
    vfs_sftpfs_ops.rename = sftpfs_rename;
    vfs_sftpfs_ops.ferrno = sftpfs_errno;
    vfs_sftpfs_ops.lseek = sftpfs_lseek;
    vfs_sftpfs_ops.mknod = sftpfs_mknod;
    vfs_sftpfs_ops.mkdir = sftpfs_mkdir;
    vfs_sftpfs_ops.rmdir = sftpfs_rmdir;
    vfs_register_class (&vfs_sftpfs_ops);

    mc_event_add ("vfs", "plugin_name_for_config_dialog", sftpfs_plugin_name_for_config_dialog,
                  NULL, NULL);
    mc_event_add ("vfs", "plugin_show_config_dialog", sftpfs_plugin_show_config_dialog, NULL, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
sftpfs_load_param (const char *section_name)
{
    char *profile;
    char *buffer;
    mc_config_t *sftpfs_config;

    profile = g_build_filename (mc_config_get_path (), "hotlist.ini", NULL);
    sftpfs_config = mc_config_init (profile);
    g_free (profile);

    if (sftpfs_config == NULL)
        return;

    if (!mc_config_has_group (sftpfs_config, section_name))
        sftpfs_newcon = TRUE;

    buffer = mc_config_get_string (sftpfs_config, section_name, "private_key_path", "~/.ssh/key_name");
    if (buffer != NULL && buffer[0] != '\0')
        sftpfs_privkey = g_strdup (buffer);
    g_free (buffer);

    buffer = mc_config_get_string (sftpfs_config, section_name, "public_key_path", "~/.ssh/key_name.pub");
    if (buffer != NULL && buffer[0] != '\0')
        sftpfs_pubkey = g_strdup (buffer);
    g_free (buffer);

    sftpfs_auth_method = mc_config_get_int (sftpfs_config, section_name, "auth", SFTP_AUTH_AUTO);

    buffer = mc_config_get_string (sftpfs_config, section_name, "user", "");
    if (buffer != NULL && buffer[0] != '\0')
        sftpfs_user = g_strdup (buffer);
    g_free (buffer);

    sftpfs_port = mc_config_get_int (sftpfs_config, section_name, "port", SFTP_DEFAULT_PORT);
    mc_config_deinit (sftpfs_config);
}

/* --------------------------------------------------------------------------------------------- */

void
sftpfs_save_param (const char *section_name)
{
    char *profile;
    mc_config_t *sftpfs_config;

    profile = g_build_filename (mc_config_get_path (), "hotlist.ini", NULL);
    sftpfs_config = mc_config_init (profile);
    g_free (profile);

    if (sftpfs_config == NULL)
        return;

    sftpfs_newcon = FALSE;
    mc_config_set_string (sftpfs_config, section_name, "proto", "sftp");
    mc_config_set_string (sftpfs_config, section_name, "private_key_path", sftpfs_privkey);
    mc_config_set_string (sftpfs_config, section_name, "public_key_path", sftpfs_pubkey);
    mc_config_set_string (sftpfs_config, section_name, "user", sftpfs_user);
    mc_config_set_string (sftpfs_config, section_name, "host", sftpfs_host);
    mc_config_set_int (sftpfs_config, section_name, "auth", sftpfs_auth_method);
    mc_config_set_int (sftpfs_config, section_name, "port", sftpfs_port);
    mc_config_save_file (sftpfs_config, NULL);
    mc_config_deinit (sftpfs_config);
}

/* --------------------------------------------------------------------------------------------- */
