/*
   Search functions for diffviewer.

   Copyright (C) 2010-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.
   Andrew Borodin <aborodin@vmail.ru>, 2012

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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/strescape.h"      /* strutils_glob_escape() */
#include "lib/vfs/vfs.h"        /* mc_opendir, mc_readdir, mc_closedir, */
#include "lib/widget.h"
#include "lib/util.h"

#include "execute.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define FILE_READ_BUF 4096
#define FILE_FLAG_TEMP (1 << 0)

#define FILE_DIRTY(fs) \
do \
{ \
    (fs)->pos = 0; \
    (fs)->len = 0;  \
} \
while (0)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Free file structure without closing the file.
 *
 * @param fs file structure
 * @return 0 on success, non-zero on error
 */

static int
f_free (FBUF * fs)
{
    int rv = 0;

    if (fs->flags & FILE_FLAG_TEMP)
    {
        rv = unlink (fs->data);
        g_free (fs->data);
    }
    g_free (fs->buf);
    g_free (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create pipe stream to process.
 *
 * @param cmd shell command line
 * @param flags open mode, either O_RDONLY or O_WRONLY
 *
 * @return file structure
 */

static FBUF *
p_open (const char *cmd, int flags)
{
    FILE *f;
    FBUF *fs;
    const char *type = NULL;

    if (flags == O_RDONLY)
        type = "r";
    else if (flags == O_WRONLY)
        type = "w";

    if (type == NULL)
        return NULL;

    fs = mc_diffviewer_file_dopen (0);
    if (fs == NULL)
        return NULL;

    f = popen (cmd, type);
    if (f == NULL)
    {
        f_free (fs);
        return NULL;
    }

    fs->fd = fileno (f);
    fs->data = f;
    return fs;
}

/* --------------------------------------------------------------------------------------------- */
/* diff parse *************************************************************** */
/**
 * Parse line for diff statement.
 *
 * @param p string to parse
 * @param ops list of diff statements
 * @return 0 if success, otherwise non-zero
 */

static int
scan_line (const char *p, GArray * ops)
{
    DIFFCMD op;

    int f1, f2;
    int t1, t2;
    int cmd;
    int range;

    /* handle the following cases:
     *  NUMaNUM[,NUM]
     *  NUM[,NUM]cNUM[,NUM]
     *  NUM[,NUM]dNUM
     * where NUM is a positive integer
     */

    if (mc_diffviewer_scan_deci (&p, &f1) != 0 || f1 < 0)
        return -1;

    f2 = f1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (mc_diffviewer_scan_deci (&p, &f2) != 0 || f2 < f1)
            return -1;

        range = 1;
    }

    cmd = *p++;
    if (cmd == 'a')
    {
        if (range != 0)
            return -1;
    }
    else if (cmd != 'c' && cmd != 'd')
        return -1;

    if (mc_diffviewer_scan_deci (&p, &t1) != 0 || t1 < 0)
        return -1;

    t2 = t1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (mc_diffviewer_scan_deci (&p, &t2) != 0 || t2 < t1)
            return -1;

        range = 1;
    }

    if (cmd == 'd' && range != 0)
        return -1;

    op.a[0][0] = f1;
    op.a[0][1] = f2;
    op.cmd = cmd;
    op.a[1][0] = t1;
    op.a[1][1] = t2;
    g_array_append_val (ops, op);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Parse diff output and extract diff statements.
 *
 * @param f stream to read from
 * @param ops list of diff statements to fill
 * @return positive number indicating number of hunks, otherwise negative
 */

static int
scan_diff (FBUF * f, GArray * ops)
{
    int sz;
    char buf[BUFSIZ];

    while ((sz = mc_diffviewer_file_gets (buf, sizeof (buf) - 1, f)) != 0)
    {
        if (isdigit (buf[0]))
        {
            if (buf[sz - 1] != '\n')
                return -1;

            buf[sz] = '\0';
            if (scan_line (buf, ops) != 0)
                return -1;

            continue;
        }

        while (buf[sz - 1] != '\n' && (sz = mc_diffviewer_file_gets (buf, sizeof (buf), f)) != 0)
            ;
    }

    return ops->len;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Close pipe stream.
 *
 * @param fs structure
 * @return 0 on success, non-zero on error
 */

static int
p_close (FBUF * fs)
{
    int rv = -1;

    if (fs != NULL)
    {
        rv = pclose (fs->data);
        f_free (fs);
    }

    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Invoke diff and extract diff statements.
 *
 * @param args extra arguments to be passed to diff
 * @param extra more arguments to be passed to diff
 * @param file1 first file to compare
 * @param file2 second file to compare
 * @param ops list of diff statements to fill
 *
 * @return positive number indicating number of hunks, otherwise negative
 */

int
mc_diffviewer_execute (const char *args, const char *extra, const char *file1, const char *file2,
                       GArray * ops)
{
    static const char *opt =
        " --old-group-format='%df%(f=l?:,%dl)d%dE\n'"
        " --new-group-format='%dea%dF%(F=L?:,%dL)\n'"
        " --changed-group-format='%df%(f=l?:,%dl)c%dF%(F=L?:,%dL)\n'"
        " --unchanged-group-format=''";

    int rv;
    FBUF *f;
    char *cmd;
    int code;
    char *file1_esc, *file2_esc;

    /* escape potential $ to avoid shell variable substitutions in popen() */
    file1_esc = strutils_shell_escape (file1);
    file2_esc = strutils_shell_escape (file2);
    cmd = g_strdup_printf ("diff %s %s %s %s %s", args, extra, opt, file1_esc, file2_esc);
    g_free (file1_esc);
    g_free (file2_esc);

    if (cmd == NULL)
        return -1;

    f = p_open (cmd, O_RDONLY);
    g_free (cmd);

    if (f == NULL)
        return -1;

    rv = scan_diff (f, ops);
    code = p_close (f);

    if (rv < 0 || code == -1 || !WIFEXITED (code) || WEXITSTATUS (code) == 2)
        rv = -1;

    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Close file.
 * @note if this is temporary file, it is deleted
 *
 * @param fs file structure
 * @return 0 on success, non-zero on error
 */

int
mc_diffviewer_file_close (FBUF * fs)
{
    int rv = -1;

    if (fs != NULL)
    {
        rv = close (fs->fd);
        f_free (fs);
    }

    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read decimal number from string.
 *
 * @param[in,out] str string to parse
 * @param[out] n extracted number
 * @return 0 if success, otherwise non-zero
 */

int
mc_diffviewer_scan_deci (const char **str, int *n)
{
    const char *p = *str;
    char *q;

    errno = 0;
    *n = strtol (p, &q, 10);
    if (errno != 0 || p == q)
        return -1;
    *str = q;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Seek into file.
 * @note avoids thrashing read cache when possible
 *
 * @param fs file structure
 * @param off offset
 * @param whence seek directive: SEEK_SET, SEEK_CUR or SEEK_END
 *
 * @return position in file, starting from begginning
 */

off_t
mc_diffviewer_file_seek (FBUF * fs, off_t off, int whence)
{
    off_t rv;

    if (fs->len && whence != SEEK_END)
    {
        rv = lseek (fs->fd, 0, SEEK_CUR);
        if (rv != -1)
        {
            if (whence == SEEK_CUR)
            {
                whence = SEEK_SET;
                off += rv - fs->len + fs->pos;
            }
            if (off - rv >= -fs->len && off - rv <= 0)
            {
                fs->pos = fs->len + off - rv;
                return off;
            }
        }
    }

    rv = lseek (fs->fd, off, whence);
    if (rv != -1)
        FILE_DIRTY (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read a line of bytes from file until newline or EOF.
 * @note does not stop on null-byte
 * @note buf will not be null-terminated
 *
 * @param buf destination buffer
 * @param size size of buffer
 * @param fs file structure
 *
 * @return number of bytes read
 */

size_t
mc_diffviewer_file_gets (char *buf, size_t size, FBUF * fs)
{
    size_t j = 0;

    do
    {
        int i;
        int stop = 0;

        for (i = fs->pos; j < size && i < fs->len && !stop; i++, j++)
        {
            buf[j] = fs->buf[i];
            if (buf[j] == '\n')
                stop = 1;
        }
        fs->pos = i;

        if (j == size || stop)
            break;

        fs->pos = 0;
        fs->len = read (fs->fd, fs->buf, FILE_READ_BUF);
    }
    while (fs->len > 0);

    return j;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Seek to the beginning of file, thrashing read cache.
 *
 * @param fs file structure
 *
 * @return 0 if success, non-zero on error
 */

off_t
mc_diffviewer_file_reset (FBUF * fs)
{
    off_t rv;

    rv = lseek (fs->fd, 0, SEEK_SET);
    if (rv != -1)
        FILE_DIRTY (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Open a binary file in specified mode.
 *
 * @param filename file name
 * @param flags open mode, a combination of O_RDONLY, O_WRONLY, O_RDWR
 *
 * @return file structure
 */

FBUF *
mc_diffviewer_file_open (const char *filename, int flags)
{
    int fd;
    FBUF *fs;

    fs = mc_diffviewer_file_dopen (0);
    if (fs == NULL)
        return NULL;

    fd = open (filename, flags);
    if (fd < 0)
    {
        f_free (fs);
        return NULL;
    }

    fs->fd = fd;
    return fs;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Write bytes to file.
 * @note thrashes read cache
 *
 * @param fs file structure
 * @param buf source buffer
 * @param size size of buffer
 *
 * @return number of written bytes, -1 on error
 */

ssize_t
mc_diffviewer_file_write (FBUF * fs, const char *buf, size_t size)
{
    ssize_t rv;

    rv = write (fs->fd, buf, size);
    if (rv >= 0)
        FILE_DIRTY (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Truncate file to the current position.
 * @note thrashes read cache
 *
 * @param fs file structure
 *
 * @return current file size on success, negative on error
 */

off_t
mc_diffviewer_file_trunc (FBUF * fs)
{
    off_t off;

    off = lseek (fs->fd, 0, SEEK_CUR);
    if (off != -1)
    {
        int rv;

        rv = ftruncate (fs->fd, off);
        if (rv != 0)
            off = -1;
        else
            FILE_DIRTY (fs);
    }
    return off;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Alocate file structure and associate file descriptor to it.
 *
 * @param fd file descriptor
 * @return file structure
 */

FBUF *
mc_diffviewer_file_dopen (int fd)
{
    FBUF *fs;

    if (fd < 0)
        return NULL;

    fs = g_try_malloc (sizeof (FBUF));
    if (fs == NULL)
        return NULL;

    fs->buf = g_try_malloc (FILE_READ_BUF);
    if (fs->buf == NULL)
    {
        g_free (fs);
        return NULL;
    }

    fs->fd = fd;
    FILE_DIRTY (fs);
    fs->flags = 0;
    fs->data = NULL;

    return fs;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to open a temporary file.
 * @note the name is not altered if this function fails
 *
 * @param[out] name address of a pointer to store the temporary name
 * @return file descriptor on success, negative on error
 */

static int
open_temp (void **name)
{
    int fd;
    vfs_path_t *diff_file_name = NULL;

    fd = mc_mkstemps (&diff_file_name, "mcdiff", NULL);
    if (fd == -1)
    {
        message (D_ERROR, MSG_ERROR,
                 _("Cannot create temporary diff file\n%s"), unix_error_string (errno));
        return -1;
    }
    *name = g_strdup (vfs_path_as_str (diff_file_name));
    vfs_path_free (diff_file_name);
    return fd;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Open a binary temporary file in R/W mode.
 * @note the file will be deleted when closed
 *
 * @return file structure
 */

FBUF *
mc_diffviewer_file_temp (void)
{
    int fd;
    FBUF *fs;

    fs = mc_diffviewer_file_dopen (0);
    if (fs == NULL)
        return NULL;

    fd = open_temp (&fs->data);
    if (fd < 0)
    {
        f_free (fs);
        return NULL;
    }

    fs->fd = fd;
    fs->flags = FILE_FLAG_TEMP;
    return fs;
}

/* --------------------------------------------------------------------------------------------- */
