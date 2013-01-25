/*
   Copyright (C) 2007, 2010, 2011, 2012
   The Free Software Foundation, Inc.

   Written by:
   Daniel Borca <dborca@yahoo.com>, 2007
   Slava Zanko <slavazanko@gmail.com>, 2010
   Andrew Borodin <aborodin@vmail.ru>, 2010, 2012
   Ilia Maslakov <il.smind@gmail.com>, 2010

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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/tty/key.h"
#include "lib/skin.h"           /* EDITOR_NORMAL_COLOR */
#include "lib/vfs/vfs.h"        /* mc_opendir, mc_readdir, mc_closedir, */
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/strutil.h"
#include "lib/strescape.h"      /* strutils_glob_escape() */
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/event.h"          /* mc_event_raise() */

#include "src/filemanager/cmd.h"        /* do_edit_at_line(), view_other_cmd() */
#include "src/filemanager/panel.h"
#include "src/filemanager/layout.h"     /* Needed for get_current_index and get_other_panel */

#include "src/keybind-defaults.h"
#include "src/setup.h"
#include "src/history.h"
#ifdef HAVE_CHARSET
#include "src/selcodepage.h"
#endif

#include "ydiff.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define g_array_foreach(a, TP, cbf) \
do { \
    size_t g_array_foreach_i;\
    TP *g_array_foreach_var = NULL; \
    for (g_array_foreach_i = 0; g_array_foreach_i < a->len; g_array_foreach_i++) \
    { \
        g_array_foreach_var = &g_array_index (a, TP, g_array_foreach_i); \
        (*cbf) (g_array_foreach_var); \
    } \
} while (0)

#define FILE_READ_BUF 4096
#define FILE_FLAG_TEMP (1 << 0)

#define ADD_CH '+'
#define DEL_CH '-'
#define CHG_CH '*'
#define EQU_CH ' '

#define HDIFF_ENABLE 1
#define HDIFF_MINCTX 5
#define HDIFF_DEPTH 10

#define FILE_DIRTY(fs) \
do \
{ \
    (fs)->pos = 0; \
    (fs)->len = 0;  \
} \
while (0)

/*** file scope type declarations ****************************************************************/

typedef enum
{
    FROM_LEFT_TO_RIGHT,
    FROM_RIGHT_TO_LEFT
} action_direction_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline int
TAB_SKIP (int ts, int pos)
{
    if (ts > 0 && ts < 9)
        return ts - pos % ts;
    else
        return 8 - pos % 8;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
rewrite_backup_content (const vfs_path_t * from_file_name_vpath, const char *to_file_name)
{
    FILE *backup_fd;
    char *contents;
    gsize length;
    const char *from_file_name;

    from_file_name = vfs_path_get_by_index (from_file_name_vpath, -1)->path;
    if (!g_file_get_contents (from_file_name, &contents, &length, NULL))
        return FALSE;

    backup_fd = fopen (to_file_name, "w");
    if (backup_fd == NULL)
    {
        g_free (contents);
        return FALSE;
    }

    length = fwrite ((const void *) contents, length, 1, backup_fd);

    fflush (backup_fd);
    fclose (backup_fd);
    g_free (contents);
    return TRUE;
}

/* buffered I/O ************************************************************* */

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
    *name = vfs_path_to_str (diff_file_name);
    vfs_path_free (diff_file_name);
    return fd;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Alocate file structure and associate file descriptor to it.
 *
 * @param fd file descriptor
 * @return file structure
 */

static FBUF *
f_dopen (int fd)
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
 * Open a binary temporary file in R/W mode.
 * @note the file will be deleted when closed
 *
 * @return file structure
 */
static FBUF *
f_temp (void)
{
    int fd;
    FBUF *fs;

    fs = f_dopen (0);
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

/**
 * Open a binary file in specified mode.
 *
 * @param filename file name
 * @param flags open mode, a combination of O_RDONLY, O_WRONLY, O_RDWR
 *
 * @return file structure
 */

static FBUF *
f_open (const char *filename, int flags)
{
    int fd;
    FBUF *fs;

    fs = f_dopen (0);
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

static size_t
f_gets (char *buf, size_t size, FBUF * fs)
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
 * Seek into file.
 * @note avoids thrashing read cache when possible
 *
 * @param fs file structure
 * @param off offset
 * @param whence seek directive: SEEK_SET, SEEK_CUR or SEEK_END
 *
 * @return position in file, starting from begginning
 */

static off_t
f_seek (FBUF * fs, off_t off, int whence)
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
 * Seek to the beginning of file, thrashing read cache.
 *
 * @param fs file structure
 *
 * @return 0 if success, non-zero on error
 */

static off_t
f_reset (FBUF * fs)
{
    off_t rv;

    rv = lseek (fs->fd, 0, SEEK_SET);
    if (rv != -1)
        FILE_DIRTY (fs);
    return rv;
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

static ssize_t
f_write (FBUF * fs, const char *buf, size_t size)
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

static off_t
f_trunc (FBUF * fs)
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
 * Close file.
 * @note if this is temporary file, it is deleted
 *
 * @param fs file structure
 * @return 0 on success, non-zero on error
 */

static int
f_close (FBUF * fs)
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

    fs = f_dopen (0);
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

/**
 * Get one char (byte) from string
 *
 * @param char * str, gboolean * result
 * @return int as character or 0 and result == FALSE if fail
 */

static int
dview_get_byte (char *str, gboolean * result)
{
    if (str == NULL)
    {
        *result = FALSE;
        return 0;
    }
    *result = TRUE;
    return (unsigned char) *str;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get utf multibyte char from string
 *
 * @param char * str, int * char_width, gboolean * result
 * @return int as utf character or 0 and result == FALSE if fail
 */

static int
dview_get_utf (char *str, int *char_width, gboolean * result)
{
    int res = -1;
    gunichar ch;
    gchar *next_ch = NULL;
    int width = 0;

    *result = TRUE;

    if (str == NULL)
    {
        *result = FALSE;
        return 0;
    }

    res = g_utf8_get_char_validated (str, -1);

    if (res < 0)
        ch = *str;
    else
    {
        ch = res;
        /* Calculate UTF-8 char width */
        next_ch = g_utf8_next_char (str);
        if (next_ch != NULL)
            width = next_ch - str;
        else
            ch = 0;
    }
    *char_width = width;
    return ch;
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_str_utf8_offset_to_pos (const char *text, size_t length)
{
    ptrdiff_t result;

    if (text == NULL || text[0] == '\0')
        return length;

    if (g_utf8_validate (text, -1, NULL))
        result = g_utf8_offset_to_pointer (text, length) - text;
    else
    {
        gunichar uni;
        char *tmpbuf, *buffer;

        buffer = tmpbuf = g_strdup (text);
        while (tmpbuf[0] != '\0')
        {
            uni = g_utf8_get_char_validated (tmpbuf, -1);
            if ((uni != (gunichar) (-1)) && (uni != (gunichar) (-2)))
                tmpbuf = g_utf8_next_char (tmpbuf);
            else
            {
                tmpbuf[0] = '.';
                tmpbuf++;
            }
        }
        result = g_utf8_offset_to_pointer (tmpbuf, length) - tmpbuf;
        g_free (buffer);
    }
    return max (length, (size_t) result);
}

/* --------------------------------------------------------------------------------------------- */

/* diff parse *************************************************************** */

/**
 * Read decimal number from string.
 *
 * @param[in,out] str string to parse
 * @param[out] n extracted number
 * @return 0 if success, otherwise non-zero
 */

static int
scan_deci (const char **str, int *n)
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

    if (scan_deci (&p, &f1) != 0 || f1 < 0)
        return -1;

    f2 = f1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (scan_deci (&p, &f2) != 0 || f2 < f1)
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

    if (scan_deci (&p, &t1) != 0 || t1 < 0)
        return -1;

    t2 = t1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (scan_deci (&p, &t2) != 0 || t2 < t1)
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

    while ((sz = f_gets (buf, sizeof (buf) - 1, f)) != 0)
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

        while (buf[sz - 1] != '\n' && (sz = f_gets (buf, sizeof (buf), f)) != 0)
            ;
    }

    return ops->len;
}

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

static int
dff_execute (const char *args, const char *extra, const char *file1, const char *file2,
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
 * Reparse and display file according to diff statements.
 *
 * @param ord DIFF_LEFT if 1nd file is displayed , DIFF_RIGHT if 2nd file is displayed.
 * @param filename file name to display
 * @param ops list of diff statements
 * @param printer printf-like function to be used for displaying
 * @param ctx printer context
 *
 * @return 0 if success, otherwise non-zero
 */

static int
dff_reparse (diff_place_t ord, const char *filename, const GArray * ops, DFUNC printer, void *ctx)
{
    size_t i;
    FBUF *f;
    size_t sz;
    char buf[BUFSIZ];
    int line = 0;
    off_t off = 0;
    const DIFFCMD *op;
    diff_place_t eff;
    int add_cmd;
    int del_cmd;

    f = f_open (filename, O_RDONLY);
    if (f == NULL)
        return -1;

    ord &= 1;
    eff = ord;

    add_cmd = 'a';
    del_cmd = 'd';
    if (ord != 0)
    {
        add_cmd = 'd';
        del_cmd = 'a';
    }
#define F1 a[eff][0]
#define F2 a[eff][1]
#define T1 a[ ord^1 ][0]
#define T2 a[ ord^1 ][1]
    for (i = 0; i < ops->len; i++)
    {
        int n;

        op = &g_array_index (ops, DIFFCMD, i);
        n = op->F1 - (op->cmd != add_cmd);

        while (line < n && (sz = f_gets (buf, sizeof (buf), f)) != 0)
        {
            line++;
            printer (ctx, EQU_CH, line, off, sz, buf);
            off += sz;
            while (buf[sz - 1] != '\n')
            {
                sz = f_gets (buf, sizeof (buf), f);
                if (sz == 0)
                {
                    printer (ctx, 0, 0, 0, 1, "\n");
                    break;
                }
                printer (ctx, 0, 0, 0, sz, buf);
                off += sz;
            }
        }

        if (line != n)
            goto err;

        if (op->cmd == add_cmd)
        {
            n = op->T2 - op->T1 + 1;
            while (n != 0)
            {
                printer (ctx, DEL_CH, 0, 0, 1, "\n");
                n--;
            }
        }

        if (op->cmd == del_cmd)
        {
            n = op->F2 - op->F1 + 1;
            while (n != 0 && (sz = f_gets (buf, sizeof (buf), f)) != 0)
            {
                line++;
                printer (ctx, ADD_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    sz = f_gets (buf, sizeof (buf), f);
                    if (sz == 0)
                    {
                        printer (ctx, 0, 0, 0, 1, "\n");
                        break;
                    }
                    printer (ctx, 0, 0, 0, sz, buf);
                    off += sz;
                }
                n--;
            }

            if (n != 0)
                goto err;
        }

        if (op->cmd == 'c')
        {
            n = op->F2 - op->F1 + 1;
            while (n != 0 && (sz = f_gets (buf, sizeof (buf), f)) != 0)
            {
                line++;
                printer (ctx, CHG_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    sz = f_gets (buf, sizeof (buf), f);
                    if (sz == 0)
                    {
                        printer (ctx, 0, 0, 0, 1, "\n");
                        break;
                    }
                    printer (ctx, 0, 0, 0, sz, buf);
                    off += sz;
                }
                n--;
            }

            if (n != 0)
                goto err;

            n = op->T2 - op->T1 - (op->F2 - op->F1);
            while (n > 0)
            {
                printer (ctx, CHG_CH, 0, 0, 1, "\n");
                n--;
            }
        }
    }
#undef T2
#undef T1
#undef F2
#undef F1

    while ((sz = f_gets (buf, sizeof (buf), f)) != 0)
    {
        line++;
        printer (ctx, EQU_CH, line, off, sz, buf);
        off += sz;
        while (buf[sz - 1] != '\n')
        {
            sz = f_gets (buf, sizeof (buf), f);
            if (sz == 0)
            {
                printer (ctx, 0, 0, 0, 1, "\n");
                break;
            }
            printer (ctx, 0, 0, 0, sz, buf);
            off += sz;
        }
    }

    f_close (f);
    return 0;

  err:
    f_close (f);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* horizontal diff ********************************************************** */

/**
 * Longest common substring.
 *
 * @param s first string
 * @param m length of first string
 * @param t second string
 * @param n length of second string
 * @param ret list of offsets for longest common substrings inside each string
 * @param min minimum length of common substrings
 *
 * @return 0 if success, nonzero otherwise
 */

static int
lcsubstr (const char *s, int m, const char *t, int n, GArray * ret, int min)
{
    int i, j;
    int *Lprev, *Lcurr;
    int z = 0;

    if (m < min || n < min)
    {
        /* XXX early culling */
        return 0;
    }

    Lprev = g_try_new0 (int, n + 1);
    if (Lprev == NULL)
        return -1;

    Lcurr = g_try_new0 (int, n + 1);
    if (Lcurr == NULL)
    {
        g_free (Lprev);
        return -1;
    }

    for (i = 0; i < m; i++)
    {
        int *L;

        L = Lprev;
        Lprev = Lcurr;
        Lcurr = L;
#ifdef USE_MEMSET_IN_LCS
        memset (Lcurr, 0, (n + 1) * sizeof (int));
#endif
        for (j = 0; j < n; j++)
        {
#ifndef USE_MEMSET_IN_LCS
            Lcurr[j + 1] = 0;
#endif
            if (s[i] == t[j])
            {
                int v;

                v = Lprev[j] + 1;
                Lcurr[j + 1] = v;
                if (z < v)
                {
                    z = v;
                    g_array_set_size (ret, 0);
                }
                if (z == v && z >= min)
                {
                    int off0, off1;
                    size_t k;

                    off0 = i - z + 1;
                    off1 = j - z + 1;

                    for (k = 0; k < ret->len; k++)
                    {
                        PAIR *p = (PAIR *) g_array_index (ret, PAIR, k);
                        if ((*p)[0] == off0 || (*p)[1] >= off1)
                            break;
                    }
                    if (k == ret->len)
                    {
                        PAIR p2;

                        p2[0] = off0;
                        p2[1] = off1;
                        g_array_append_val (ret, p2);
                    }
                }
            }
        }
    }

    free (Lcurr);
    free (Lprev);
    return z;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Scan recursively for common substrings and build ranges.
 *
 * @param s first string
 * @param t second string
 * @param bracket current limits for both of the strings
 * @param min minimum length of common substrings
 * @param hdiff list of horizontal diff ranges to fill
 * @param depth recursion depth
 *
 * @return 0 if success, nonzero otherwise
 */

static gboolean
hdiff_multi (const char *s, const char *t, const BRACKET bracket, int min, GArray * hdiff,
             unsigned int depth)
{
    BRACKET p;

    if (depth-- != 0)
    {
        GArray *ret;
        BRACKET b;
        int len;

        ret = g_array_new (FALSE, TRUE, sizeof (PAIR));
        if (ret == NULL)
            return FALSE;

        len = lcsubstr (s + bracket[DIFF_LEFT].off, bracket[DIFF_LEFT].len,
                        t + bracket[DIFF_RIGHT].off, bracket[DIFF_RIGHT].len, ret, min);
        if (ret->len != 0)
        {
            size_t k = 0;
            const PAIR *data = (const PAIR *) &g_array_index (ret, PAIR, 0);
            const PAIR *data2;

            b[DIFF_LEFT].off = bracket[DIFF_LEFT].off;
            b[DIFF_LEFT].len = (*data)[0];
            b[DIFF_RIGHT].off = bracket[DIFF_RIGHT].off;
            b[DIFF_RIGHT].len = (*data)[1];
            if (!hdiff_multi (s, t, b, min, hdiff, depth))
                return FALSE;

            for (k = 0; k < ret->len - 1; k++)
            {
                data = (const PAIR *) &g_array_index (ret, PAIR, k);
                data2 = (const PAIR *) &g_array_index (ret, PAIR, k + 1);
                b[DIFF_LEFT].off = bracket[DIFF_LEFT].off + (*data)[0] + len;
                b[DIFF_LEFT].len = (*data2)[0] - (*data)[0] - len;
                b[DIFF_RIGHT].off = bracket[DIFF_RIGHT].off + (*data)[1] + len;
                b[DIFF_RIGHT].len = (*data2)[1] - (*data)[1] - len;
                if (!hdiff_multi (s, t, b, min, hdiff, depth))
                    return FALSE;
            }
            data = (const PAIR *) &g_array_index (ret, PAIR, k);
            b[DIFF_LEFT].off = bracket[DIFF_LEFT].off + (*data)[0] + len;
            b[DIFF_LEFT].len = bracket[DIFF_LEFT].len - (*data)[0] - len;
            b[DIFF_RIGHT].off = bracket[DIFF_RIGHT].off + (*data)[1] + len;
            b[DIFF_RIGHT].len = bracket[DIFF_RIGHT].len - (*data)[1] - len;
            if (!hdiff_multi (s, t, b, min, hdiff, depth))
                return FALSE;

            g_array_free (ret, TRUE);
            return TRUE;
        }
    }

    p[DIFF_LEFT].off = bracket[DIFF_LEFT].off;
    p[DIFF_LEFT].len = bracket[DIFF_LEFT].len;
    p[DIFF_RIGHT].off = bracket[DIFF_RIGHT].off;
    p[DIFF_RIGHT].len = bracket[DIFF_RIGHT].len;
    g_array_append_val (hdiff, p);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Build list of horizontal diff ranges.
 *
 * @param s first string
 * @param m length of first string
 * @param t second string
 * @param n length of second string
 * @param min minimum length of common substrings
 * @param hdiff list of horizontal diff ranges to fill
 * @param depth recursion depth
 *
 * @return 0 if success, nonzero otherwise
 */

static gboolean
hdiff_scan (const char *s, int m, const char *t, int n, int min, GArray * hdiff, unsigned int depth)
{
    int i;
    BRACKET b;

    /* dumbscan (single horizontal diff) -- does not compress whitespace */
    for (i = 0; i < m && i < n && s[i] == t[i]; i++)
        ;
    for (; m > i && n > i && s[m - 1] == t[n - 1]; m--, n--)
        ;

    b[DIFF_LEFT].off = i;
    b[DIFF_LEFT].len = m - i;
    b[DIFF_RIGHT].off = i;
    b[DIFF_RIGHT].len = n - i;

    /* smartscan (multiple horizontal diff) */
    return hdiff_multi (s, t, b, min, hdiff, depth);
}

/* --------------------------------------------------------------------------------------------- */

/* read line **************************************************************** */

/**
 * Check if character is inside horizontal diff limits.
 *
 * @param k rank of character inside line
 * @param hdiff horizontal diff structure
 * @param ord DIFF_LEFT if reading from first file, DIFF_RIGHT if reading from 2nd file
 *
 * @return TRUE if inside hdiff limits, FALSE otherwise
 */

static gboolean
is_inside (int k, GArray * hdiff, diff_place_t ord)
{
    size_t i;
    BRACKET *b;

    for (i = 0; i < hdiff->len; i++)
    {
        int start, end;

        b = &g_array_index (hdiff, BRACKET, i);
        start = (*b)[ord].off;
        end = start + (*b)[ord].len;
        if (k >= start && k < end)
            return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copy 'src' to 'dst' expanding tabs.
 * @note The procedure returns when all bytes are consumed from 'src'
 *
 * @param dst destination buffer
 * @param src source buffer
 * @param srcsize size of src buffer
 * @param base virtual base of this string, needed to calculate tabs
 * @param ts tab size
 *
 * @return new virtual base
 */

static int
cvt_cpy (char *dst, const char *src, size_t srcsize, int base, int ts)
{
    int i;

    for (i = 0; srcsize != 0; i++, src++, dst++, srcsize--)
    {
        *dst = *src;
        if (*src == '\t')
        {
            int j;

            j = TAB_SKIP (ts, i + base);
            i += j - 1;
            while (j-- > 0)
                *dst++ = ' ';
            dst--;
        }
    }
    return i + base;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copy 'src' to 'dst' expanding tabs.
 *
 * @param dst destination buffer
 * @param dstsize size of dst buffer
 * @param[in,out] _src source buffer
 * @param srcsize size of src buffer
 * @param base virtual base of this string, needed to calculate tabs
 * @param ts tab size
 *
 * @return new virtual base
 *
 * @note The procedure returns when all bytes are consumed from 'src'
 *       or 'dstsize' bytes are written to 'dst'
 * @note Upon return, 'src' points to the first unwritten character in source
 */

static int
cvt_ncpy (char *dst, int dstsize, const char **_src, size_t srcsize, int base, int ts)
{
    int i;
    const char *src = *_src;

    for (i = 0; i < dstsize && srcsize != 0; i++, src++, dst++, srcsize--)
    {
        *dst = *src;
        if (*src == '\t')
        {
            int j;

            j = TAB_SKIP (ts, i + base);
            if (j > dstsize - i)
                j = dstsize - i;
            i += j - 1;
            while (j-- > 0)
                *dst++ = ' ';
            dst--;
        }
    }
    *_src = src;
    return i + base;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read line from memory, converting tabs to spaces and padding with spaces.
 *
 * @param src buffer to read from
 * @param srcsize size of src buffer
 * @param dst buffer to read to
 * @param dstsize size of dst buffer, excluding trailing null
 * @param skip number of characters to skip
 * @param ts tab size
 * @param show_cr show trailing carriage return as ^M
 *
 * @return negative on error, otherwise number of bytes except padding
 */

static int
cvt_mget (const char *src, size_t srcsize, char *dst, int dstsize, int skip, int ts, int show_cr)
{
    int sz = 0;

    if (src != NULL)
    {
        int i;
        char *tmp = dst;
        const int base = 0;

        for (i = 0; dstsize != 0 && srcsize != 0 && *src != '\n'; i++, src++, srcsize--)
        {
            if (*src == '\t')
            {
                int j;

                j = TAB_SKIP (ts, i + base);
                i += j - 1;
                while (j-- > 0)
                {
                    if (skip > 0)
                        skip--;
                    else if (dstsize != 0)
                    {
                        dstsize--;
                        *dst++ = ' ';
                    }
                }
            }
            else if (src[0] == '\r' && (srcsize == 1 || src[1] == '\n'))
            {
                if (skip == 0 && show_cr)
                {
                    if (dstsize > 1)
                    {
                        dstsize -= 2;
                        *dst++ = '^';
                        *dst++ = 'M';
                    }
                    else
                    {
                        dstsize--;
                        *dst++ = '.';
                    }
                }
                break;
            }
            else if (skip > 0)
            {
                int utf_ch = 0;
                gboolean res;
                int w;

                skip--;
                utf_ch = dview_get_utf ((char *) src, &w, &res);
                if (w > 1)
                    skip += w - 1;
                if (!g_unichar_isprint (utf_ch))
                    utf_ch = '.';
            }
            else
            {
                dstsize--;
                *dst++ = *src;
            }
        }
        sz = dst - tmp;
    }
    while (dstsize != 0)
    {
        dstsize--;
        *dst++ = ' ';
    }
    *dst = '\0';
    return sz;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read line from memory and build attribute array.
 *
 * @param src buffer to read from
 * @param srcsize size of src buffer
 * @param dst buffer to read to
 * @param dstsize size of dst buffer, excluding trailing null
 * @param skip number of characters to skip
 * @param ts tab size
 * @param show_cr show trailing carriage return as ^M
 * @param hdiff horizontal diff structure
 * @param ord DIFF_LEFT if reading from first file, DIFF_RIGHT if reading from 2nd file
 * @param att buffer of attributes
 *
 * @return negative on error, otherwise number of bytes except padding
 */

static int
cvt_mgeta (const char *src, size_t srcsize, char *dst, int dstsize, int skip, int ts, int show_cr,
           GArray * hdiff, diff_place_t ord, char *att)
{
    int sz = 0;

    if (src != NULL)
    {
        int i, k;
        char *tmp = dst;
        const int base = 0;

        for (i = 0, k = 0; dstsize != 0 && srcsize != 0 && *src != '\n'; i++, k++, src++, srcsize--)
        {
            if (*src == '\t')
            {
                int j;

                j = TAB_SKIP (ts, i + base);
                i += j - 1;
                while (j-- > 0)
                {
                    if (skip != 0)
                        skip--;
                    else if (dstsize != 0)
                    {
                        dstsize--;
                        *att++ = is_inside (k, hdiff, ord);
                        *dst++ = ' ';
                    }
                }
            }
            else if (src[0] == '\r' && (srcsize == 1 || src[1] == '\n'))
            {
                if (skip == 0 && show_cr)
                {
                    if (dstsize > 1)
                    {
                        dstsize -= 2;
                        *att++ = is_inside (k, hdiff, ord);
                        *dst++ = '^';
                        *att++ = is_inside (k, hdiff, ord);
                        *dst++ = 'M';
                    }
                    else
                    {
                        dstsize--;
                        *att++ = is_inside (k, hdiff, ord);
                        *dst++ = '.';
                    }
                }
                break;
            }
            else if (skip != 0)
            {
                int utf_ch = 0;
                gboolean res;
                int w;

                skip--;
                utf_ch = dview_get_utf ((char *) src, &w, &res);
                if (w > 1)
                    skip += w - 1;
                if (!g_unichar_isprint (utf_ch))
                    utf_ch = '.';
            }
            else
            {
                dstsize--;
                *att++ = is_inside (k, hdiff, ord);
                *dst++ = *src;
            }
        }
        sz = dst - tmp;
    }
    while (dstsize != 0)
    {
        dstsize--;
        *att++ = '\0';
        *dst++ = ' ';
    }
    *dst = '\0';
    return sz;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read line from file, converting tabs to spaces and padding with spaces.
 *
 * @param f file stream to read from
 * @param off offset of line inside file
 * @param dst buffer to read to
 * @param dstsize size of dst buffer, excluding trailing null
 * @param skip number of characters to skip
 * @param ts tab size
 * @param show_cr show trailing carriage return as ^M
 *
 * @return negative on error, otherwise number of bytes except padding
 */

static int
cvt_fget (FBUF * f, off_t off, char *dst, size_t dstsize, int skip, int ts, int show_cr)
{
    int base = 0;
    int old_base = base;
    size_t amount = dstsize;
    size_t useful, offset;
    size_t i;
    size_t sz;
    int lastch = '\0';
    const char *q = NULL;
    char tmp[BUFSIZ];           /* XXX capacity must be >= max{dstsize + 1, amount} */
    char cvt[BUFSIZ];           /* XXX capacity must be >= MAX_TAB_WIDTH * amount */

    if (sizeof (tmp) < amount || sizeof (tmp) <= dstsize || sizeof (cvt) < 8 * amount)
    {
        /* abnormal, but avoid buffer overflow */
        memset (dst, ' ', dstsize);
        dst[dstsize] = '\0';
        return 0;
    }

    f_seek (f, off, SEEK_SET);

    while (skip > base)
    {
        old_base = base;
        sz = f_gets (tmp, amount, f);
        if (sz == 0)
            break;

        base = cvt_cpy (cvt, tmp, sz, old_base, ts);
        if (cvt[base - old_base - 1] == '\n')
        {
            q = &cvt[base - old_base - 1];
            base = old_base + q - cvt + 1;
            break;
        }
    }

    if (base < skip)
    {
        memset (dst, ' ', dstsize);
        dst[dstsize] = '\0';
        return 0;
    }

    useful = base - skip;
    offset = skip - old_base;

    if (useful <= dstsize)
    {
        if (useful != 0)
            memmove (dst, cvt + offset, useful);

        if (q == NULL)
        {
            sz = f_gets (tmp, dstsize - useful + 1, f);
            if (sz != 0)
            {
                const char *ptr = tmp;

                useful += cvt_ncpy (dst + useful, dstsize - useful, &ptr, sz, base, ts) - base;
                if (ptr < tmp + sz)
                    lastch = *ptr;
            }
        }
        sz = useful;
    }
    else
    {
        memmove (dst, cvt + offset, dstsize);
        sz = dstsize;
        lastch = cvt[offset + dstsize];
    }

    dst[sz] = lastch;
    for (i = 0; i < sz && dst[i] != '\n'; i++)
    {
        if (dst[i] == '\r' && dst[i + 1] == '\n')
        {
            if (show_cr)
            {
                if (i + 1 < dstsize)
                {
                    dst[i++] = '^';
                    dst[i++] = 'M';
                }
                else
                {
                    dst[i++] = '*';
                }
            }
            break;
        }
    }

    for (; i < dstsize; i++)
        dst[i] = ' ';
    dst[i] = '\0';
    return sz;
}

/* --------------------------------------------------------------------------------------------- */
/* diff printers et al ****************************************************** */

static void
cc_free_elt (void *elt)
{
    DIFFLN *p = elt;

    if (p != NULL)
        g_free (p->p);
}

/* --------------------------------------------------------------------------------------------- */

static int
printer (void *ctx, int ch, int line, off_t off, size_t sz, const char *str)
{
    GArray *a = ((PRINTER_CTX *) ctx)->a;
    DSRC dsrc = ((PRINTER_CTX *) ctx)->dsrc;

    if (ch != 0)
    {
        DIFFLN p;

        p.p = NULL;
        p.ch = ch;
        p.line = line;
        p.u.off = off;
        if (dsrc == DATA_SRC_MEM && line != 0)
        {
            if (sz != 0 && str[sz - 1] == '\n')
                sz--;
            if (sz > 0)
                p.p = g_strndup (str, sz);
            p.u.len = sz;
        }
        g_array_append_val (a, p);
    }
    else if (dsrc == DATA_SRC_MEM)
    {
        DIFFLN *p;

        p = &g_array_index (a, DIFFLN, a->len - 1);
        if (sz != 0 && str[sz - 1] == '\n')
            sz--;
        if (sz != 0)
        {
            size_t new_size;
            char *q;

            new_size = p->u.len + sz;
            q = g_realloc (p->p, new_size);
            memcpy (q + p->u.len, str, sz);
            p->p = q;
        }
        p->u.len += sz;
    }
    if (dsrc == DATA_SRC_TMP && (line != 0 || ch == 0))
    {
        FBUF *f = ((PRINTER_CTX *) ctx)->f;
        f_write (f, str, sz);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
redo_diff (WDiff * dview)
{
    FBUF *const *f = dview->f;
    PRINTER_CTX ctx;
    GArray *ops;
    int ndiff;
    int rv;
    char extra[256];

    extra[0] = '\0';
    if (dview->opt.quality == 2)
        strcat (extra, " -d");
    if (dview->opt.quality == 1)
        strcat (extra, " --speed-large-files");
    if (dview->opt.strip_trailing_cr)
        strcat (extra, " --strip-trailing-cr");
    if (dview->opt.ignore_tab_expansion)
        strcat (extra, " -E");
    if (dview->opt.ignore_space_change)
        strcat (extra, " -b");
    if (dview->opt.ignore_all_space)
        strcat (extra, " -w");
    if (dview->opt.ignore_case)
        strcat (extra, " -i");

    if (dview->dsrc != DATA_SRC_MEM)
    {
        f_reset (f[DIFF_LEFT]);
        f_reset (f[DIFF_RIGHT]);
    }

    ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    ndiff = dff_execute (dview->args, extra, dview->file[DIFF_LEFT], dview->file[DIFF_RIGHT], ops);
    if (ndiff < 0)
    {
        if (ops != NULL)
            g_array_free (ops, TRUE);
        return -1;
    }

    ctx.dsrc = dview->dsrc;

    rv = 0;
    ctx.a = dview->a[DIFF_LEFT];
    ctx.f = f[DIFF_LEFT];
    rv |= dff_reparse (DIFF_LEFT, dview->file[DIFF_LEFT], ops, printer, &ctx);

    ctx.a = dview->a[DIFF_RIGHT];
    ctx.f = f[DIFF_RIGHT];
    rv |= dff_reparse (DIFF_RIGHT, dview->file[DIFF_RIGHT], ops, printer, &ctx);

    if (ops != NULL)
        g_array_free (ops, TRUE);

    if (rv != 0 || dview->a[DIFF_LEFT]->len != dview->a[DIFF_RIGHT]->len)
        return -1;

    if (dview->dsrc == DATA_SRC_TMP)
    {
        f_trunc (f[DIFF_LEFT]);
        f_trunc (f[DIFF_RIGHT]);
    }

    if (dview->dsrc == DATA_SRC_MEM && HDIFF_ENABLE)
    {
        dview->hdiff = g_ptr_array_new ();
        if (dview->hdiff != NULL)
        {
            size_t i;
            const DIFFLN *p;
            const DIFFLN *q;

            for (i = 0; i < dview->a[DIFF_LEFT]->len; i++)
            {
                GArray *h = NULL;

                p = &g_array_index (dview->a[DIFF_LEFT], DIFFLN, i);
                q = &g_array_index (dview->a[DIFF_RIGHT], DIFFLN, i);
                if (p->line && q->line && p->ch == CHG_CH)
                {
                    h = g_array_new (FALSE, FALSE, sizeof (BRACKET));
                    if (h != NULL)
                    {
                        gboolean runresult;

                        runresult =
                            hdiff_scan (p->p, p->u.len, q->p, q->u.len, HDIFF_MINCTX, h,
                                        HDIFF_DEPTH);
                        if (!runresult)
                        {
                            g_array_free (h, TRUE);
                            h = NULL;
                        }
                    }
                }
                g_ptr_array_add (dview->hdiff, h);
            }
        }
    }
    return ndiff;
}

/* --------------------------------------------------------------------------------------------- */

static void
destroy_hdiff (WDiff * dview)
{
    if (dview->hdiff != NULL)
    {
        int i;
        int len;

        len = dview->a[DIFF_LEFT]->len;

        for (i = 0; i < len; i++)
        {
            GArray *h;

            h = (GArray *) g_ptr_array_index (dview->hdiff, i);
            if (h != NULL)
                g_array_free (h, TRUE);
        }
        g_ptr_array_free (dview->hdiff, TRUE);
        dview->hdiff = NULL;
    }

    mc_search_free (dview->search.handle);
    dview->search.handle = NULL;
    g_free (dview->search.last_string);
    dview->search.last_string = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/* stuff ******************************************************************** */

static int
get_digits (unsigned int n)
{
    int d = 1;

    while (n /= 10)
        d++;
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_line_numbers (const GArray * a, size_t pos, int *linenum, int *lineofs)
{
    const DIFFLN *p;

    *linenum = 0;
    *lineofs = 0;

    if (a->len != 0)
    {
        if (pos >= a->len)
            pos = a->len - 1;

        p = &g_array_index (a, DIFFLN, pos);

        if (p->line == 0)
        {
            int n;

            for (n = pos; n > 0; n--)
            {
                p--;
                if (p->line != 0)
                    break;
            }
            *lineofs = pos - n + 1;
        }

        *linenum = p->line;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
calc_nwidth (const GArray ** const a)
{
    int l1, o1;
    int l2, o2;

    get_line_numbers (a[DIFF_LEFT], a[DIFF_LEFT]->len - 1, &l1, &o1);
    get_line_numbers (a[DIFF_RIGHT], a[DIFF_RIGHT]->len - 1, &l2, &o2);
    if (l1 < l2)
        l1 = l2;
    return get_digits (l1);
}

/* --------------------------------------------------------------------------------------------- */

static int
find_prev_hunk (const GArray * a, int pos)
{
#if 1
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch != EQU_CH)
        pos--;
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch == EQU_CH)
        pos--;
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch != EQU_CH)
        pos--;
    if (pos > 0 && (size_t) pos < a->len)
        pos++;
#else
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos - 1))->ch == EQU_CH)
        pos--;
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos - 1))->ch != EQU_CH)
        pos--;
#endif

    return pos;
}

/* --------------------------------------------------------------------------------------------- */

static size_t
find_next_hunk (const GArray * a, size_t pos)
{
    while (pos < a->len && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch != EQU_CH)
        pos++;
    while (pos < a->len && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch == EQU_CH)
        pos++;
    return pos;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find start and end lines of the current hunk.
 *
 * @param dview WDiff widget
 * @return boolean and
 * start_line1 first line of current hunk (file[0])
 * end_line1 last line of current hunk (file[0])
 * start_line1 first line of current hunk (file[0])
 * end_line1 last line of current hunk (file[0])
 */

static int
get_current_hunk (WDiff * dview, int *start_line1, int *end_line1, int *start_line2, int *end_line2)
{
    const GArray *a0 = dview->a[DIFF_LEFT];
    const GArray *a1 = dview->a[DIFF_RIGHT];
    size_t pos;
    int ch;
    int res = 0;

    *start_line1 = 1;
    *start_line2 = 1;
    *end_line1 = 1;
    *end_line2 = 1;

    pos = dview->skip_rows;
    ch = ((DIFFLN *) & g_array_index (a0, DIFFLN, pos))->ch;
    if (ch != EQU_CH)
    {
        switch (ch)
        {
        case ADD_CH:
            res = DIFF_DEL;
            break;
        case DEL_CH:
            res = DIFF_ADD;
            break;
        case CHG_CH:
            res = DIFF_CHG;
            break;
        }
        while (pos > 0 && ((DIFFLN *) & g_array_index (a0, DIFFLN, pos))->ch != EQU_CH)
            pos--;
        if (pos > 0)
        {
            *start_line1 = ((DIFFLN *) & g_array_index (a0, DIFFLN, pos))->line + 1;
            *start_line2 = ((DIFFLN *) & g_array_index (a1, DIFFLN, pos))->line + 1;
        }
        pos = dview->skip_rows;
        while (pos < a0->len && ((DIFFLN *) & g_array_index (a0, DIFFLN, pos))->ch != EQU_CH)
        {
            int l0, l1;

            l0 = ((DIFFLN *) & g_array_index (a0, DIFFLN, pos))->line;
            l1 = ((DIFFLN *) & g_array_index (a1, DIFFLN, pos))->line;
            if (l0 > 0)
                *end_line1 = max (*start_line1, l0);
            if (l1 > 0)
                *end_line2 = max (*start_line2, l1);
            pos++;
        }
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Remove hunk from file.
 *
 * @param dview           WDiff widget
 * @param merge_file      file stream for writing data
 * @param from1           first line of hunk
 * @param to1             last line of hunk
 * @param merge_direction in what direction files should be merged
 */

static void
dview_remove_hunk (WDiff * dview, FILE * merge_file, int from1, int to1,
                   action_direction_t merge_direction)
{
    int line;
    char buf[BUF_10K];
    FILE *f0;

    if (merge_direction == FROM_RIGHT_TO_LEFT)
        f0 = fopen (dview->file[DIFF_RIGHT], "r");
    else
        f0 = fopen (dview->file[DIFF_LEFT], "r");

    line = 0;
    while (fgets (buf, sizeof (buf), f0) != NULL && line < from1 - 1)
    {
        line++;
        fputs (buf, merge_file);
    }
    while (fgets (buf, sizeof (buf), f0) != NULL)
    {
        line++;
        if (line >= to1)
            fputs (buf, merge_file);
    }
    fclose (f0);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Add hunk to file.
 *
 * @param dview           WDiff widget
 * @param merge_file      file stream for writing data
 * @param from1           first line of source hunk
 * @param from2           first line of destination hunk
 * @param to1             last line of source hunk
 * @param merge_direction in what direction files should be merged
 */

static void
dview_add_hunk (WDiff * dview, FILE * merge_file, int from1, int from2, int to2,
                action_direction_t merge_direction)
{
    int line;
    char buf[BUF_10K];
    FILE *f0;
    FILE *f1;

    if (merge_direction == FROM_RIGHT_TO_LEFT)
    {
        f0 = fopen (dview->file[DIFF_RIGHT], "r");
        f1 = fopen (dview->file[DIFF_LEFT], "r");
    }
    else
    {
        f0 = fopen (dview->file[DIFF_LEFT], "r");
        f1 = fopen (dview->file[DIFF_RIGHT], "r");
    }

    line = 0;
    while (fgets (buf, sizeof (buf), f0) != NULL && line < from1 - 1)
    {
        line++;
        fputs (buf, merge_file);
    }
    line = 0;
    while (fgets (buf, sizeof (buf), f1) != NULL && line <= to2)
    {
        line++;
        if (line >= from2)
            fputs (buf, merge_file);
    }
    while (fgets (buf, sizeof (buf), f0) != NULL)
        fputs (buf, merge_file);

    fclose (f0);
    fclose (f1);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Replace hunk in file.
 *
 * @param dview           WDiff widget
 * @param merge_file      file stream for writing data
 * @param from1           first line of source hunk
 * @param to1             last line of source hunk
 * @param from2           first line of destination hunk
 * @param to2             last line of destination hunk
 * @param merge_direction in what direction files should be merged
 */

static void
dview_replace_hunk (WDiff * dview, FILE * merge_file, int from1, int to1, int from2, int to2,
                    action_direction_t merge_direction)
{
    int line1 = 0, line2 = 0;
    char buf[BUF_10K];
    FILE *f0;
    FILE *f1;

    if (merge_direction == FROM_RIGHT_TO_LEFT)
    {
        f0 = fopen (dview->file[DIFF_RIGHT], "r");
        f1 = fopen (dview->file[DIFF_LEFT], "r");
    }
    else
    {
        f0 = fopen (dview->file[DIFF_LEFT], "r");
        f1 = fopen (dview->file[DIFF_RIGHT], "r");
    }

    while (fgets (buf, sizeof (buf), f0) != NULL && line1 < from1 - 1)
    {
        line1++;
        fputs (buf, merge_file);
    }
    while (fgets (buf, sizeof (buf), f1) != NULL && line2 <= to2)
    {
        line2++;
        if (line2 >= from2)
            fputs (buf, merge_file);
    }
    while (fgets (buf, sizeof (buf), f0) != NULL)
    {
        line1++;
        if (line1 > to1)
            fputs (buf, merge_file);
    }
    fclose (f0);
    fclose (f1);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Merge hunk.
 *
 * @param dview           WDiff widget
 * @param merge_direction in what direction files should be merged
 */

static void
do_merge_hunk (WDiff * dview, action_direction_t merge_direction)
{
    int from1, to1, from2, to2;
    int hunk;
    diff_place_t n_merge = (merge_direction == FROM_RIGHT_TO_LEFT) ? DIFF_RIGHT : DIFF_LEFT;

    if (merge_direction == FROM_RIGHT_TO_LEFT)
        hunk = get_current_hunk (dview, &from2, &to2, &from1, &to1);
    else
        hunk = get_current_hunk (dview, &from1, &to1, &from2, &to2);

    if (hunk > 0)
    {
        int merge_file_fd;
        FILE *merge_file;
        vfs_path_t *merge_file_name_vpath = NULL;

        if (!dview->merged[n_merge])
        {
            dview->merged[n_merge] = mc_util_make_backup_if_possible (dview->file[n_merge], "~~~");
            if (!dview->merged[n_merge])
            {
                message (D_ERROR, MSG_ERROR,
                         _("Cannot create backup file\n%s%s\n%s"),
                         dview->file[n_merge], "~~~", unix_error_string (errno));
                return;
            }
        }

        merge_file_fd = mc_mkstemps (&merge_file_name_vpath, "mcmerge", NULL);
        if (merge_file_fd == -1)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot create temporary merge file\n%s"),
                     unix_error_string (errno));
            return;
        }

        merge_file = fdopen (merge_file_fd, "w");

        switch (hunk)
        {
        case DIFF_DEL:
            if (merge_direction == FROM_RIGHT_TO_LEFT)
                dview_add_hunk (dview, merge_file, from1, from2, to2, FROM_RIGHT_TO_LEFT);
            else
                dview_remove_hunk (dview, merge_file, from1, to1, FROM_LEFT_TO_RIGHT);
            break;
        case DIFF_ADD:
            if (merge_direction == FROM_RIGHT_TO_LEFT)
                dview_remove_hunk (dview, merge_file, from1, to1, FROM_RIGHT_TO_LEFT);
            else
                dview_add_hunk (dview, merge_file, from1, from2, to2, FROM_LEFT_TO_RIGHT);
            break;
        case DIFF_CHG:
            dview_replace_hunk (dview, merge_file, from1, to1, from2, to2, merge_direction);
            break;
        }
        fflush (merge_file);
        fclose (merge_file);
        {
            int res;

            res = rewrite_backup_content (merge_file_name_vpath, dview->file[n_merge]);
            (void) res;
        }
        mc_unlink (merge_file_name_vpath);
        vfs_path_free (merge_file_name_vpath);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* view routines and callbacks ********************************************** */

static void
dview_compute_split (WDiff * dview, int i)
{
    dview->bias += i;
    if (dview->bias < 2 - dview->half1)
        dview->bias = 2 - dview->half1;
    if (dview->bias > dview->half2 - 2)
        dview->bias = dview->half2 - 2;
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_compute_areas (WDiff * dview)
{
    dview->height = LINES - 2;
    dview->half1 = COLS / 2;
    dview->half2 = COLS - dview->half1;

    dview_compute_split (dview, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_reread (WDiff * dview)
{
    int ndiff;

    destroy_hdiff (dview);
    if (dview->a[DIFF_LEFT] != NULL)
    {
        g_array_foreach (dview->a[DIFF_LEFT], DIFFLN, cc_free_elt);
        g_array_free (dview->a[DIFF_LEFT], TRUE);
    }
    if (dview->a[DIFF_RIGHT] != NULL)
    {
        g_array_foreach (dview->a[DIFF_RIGHT], DIFFLN, cc_free_elt);
        g_array_free (dview->a[DIFF_RIGHT], TRUE);
    }

    dview->a[DIFF_LEFT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));
    dview->a[DIFF_RIGHT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));

    ndiff = redo_diff (dview);
    if (ndiff >= 0)
        dview->ndiff = ndiff;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
static void
dview_set_codeset (WDiff * dview)
{
    const char *encoding_id = NULL;

    dview->utf8 = TRUE;
    encoding_id =
        get_codepage_id (mc_global.source_codepage >=
                         0 ? mc_global.source_codepage : mc_global.display_codepage);
    if (encoding_id != NULL)
    {
        GIConv conv;

        conv = str_crt_conv_from (encoding_id);
        if (conv != INVALID_CONV)
        {
            if (dview->converter != str_cnv_from_term)
                str_close_conv (dview->converter);
            dview->converter = conv;
        }
        dview->utf8 = (gboolean) str_isutf8 (encoding_id);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_select_encoding (WDiff * dview)
{
    if (do_select_codepage ())
        dview_set_codeset (dview);
    dview_reread (dview);
    tty_touch_screen ();
    repaint_screen ();
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

static void
dview_diff_options (WDiff * dview)
{
    const char *quality_str[] = {
        N_("No&rmal"),
        N_("&Fastest (Assume large files)"),
        N_("&Minimal (Find a smaller set of change)")
    };

    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_START_GROUPBOX (N_("Diff algorithm")),
            QUICK_RADIO (3, (const char **) quality_str, (int *) &dview->opt.quality, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("Diff extra options")),
            QUICK_CHECKBOX (N_("&Ignore case"), &dview->opt.ignore_case, NULL),
            QUICK_CHECKBOX (N_("Ignore tab &expansion"), &dview->opt.ignore_tab_expansion, NULL),
            QUICK_CHECKBOX (N_("Ignore &space change"), &dview->opt.ignore_space_change, NULL),
            QUICK_CHECKBOX (N_("Ignore all &whitespace"), &dview->opt.ignore_all_space, NULL),
            QUICK_CHECKBOX (N_("Strip &trailing carriage return"), &dview->opt.strip_trailing_cr,
                            NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 56,
        N_("Diff Options"), "[Diff Options]",
        quick_widgets, NULL, NULL
    };

    if (quick_dialog (&qdlg) != B_CANCEL)
        dview_reread (dview);
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_init (WDiff * dview, const char *args, const char *file1, const char *file2,
            const char *label1, const char *label2, DSRC dsrc)
{
    int ndiff;
    FBUF *f[DIFF_COUNT];

    f[DIFF_LEFT] = NULL;
    f[DIFF_RIGHT] = NULL;

    if (dsrc == DATA_SRC_TMP)
    {
        f[DIFF_LEFT] = f_temp ();
        if (f[DIFF_LEFT] == NULL)
            return -1;

        f[DIFF_RIGHT] = f_temp ();
        if (f[DIFF_RIGHT] == NULL)
        {
            f_close (f[DIFF_LEFT]);
            return -1;
        }
    }
    else if (dsrc == DATA_SRC_ORG)
    {
        f[DIFF_LEFT] = f_open (file1, O_RDONLY);
        if (f[DIFF_LEFT] == NULL)
            return -1;

        f[DIFF_RIGHT] = f_open (file2, O_RDONLY);
        if (f[DIFF_RIGHT] == NULL)
        {
            f_close (f[DIFF_LEFT]);
            return -1;
        }
    }

    dview->args = args;
    dview->file[DIFF_LEFT] = file1;
    dview->file[DIFF_RIGHT] = file2;
    dview->label[DIFF_LEFT] = g_strdup (label1);
    dview->label[DIFF_RIGHT] = g_strdup (label2);
    dview->f[DIFF_LEFT] = f[0];
    dview->f[DIFF_RIGHT] = f[1];
    dview->merged[DIFF_LEFT] = FALSE;
    dview->merged[DIFF_RIGHT] = FALSE;
    dview->hdiff = NULL;
    dview->dsrc = dsrc;
    dview->converter = str_cnv_from_term;
#ifdef HAVE_CHARSET
    dview_set_codeset (dview);
#endif
    dview->a[DIFF_LEFT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));
    dview->a[DIFF_RIGHT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));

    ndiff = redo_diff (dview);
    if (ndiff < 0)
    {
        /* goto MSG_DESTROY stage: dview_fini() */
        f_close (f[DIFF_LEFT]);
        f_close (f[DIFF_RIGHT]);
        return -1;
    }

    dview->ndiff = ndiff;

    dview->view_quit = 0;

    dview->bias = 0;
    dview->new_frame = 1;
    dview->skip_rows = 0;
    dview->skip_cols = 0;
    dview->display_symbols = 0;
    dview->display_numbers = 0;
    dview->show_cr = 1;
    dview->tab_size = 8;
    dview->ord = DIFF_LEFT;
    dview->full = 0;

    dview->search.handle = NULL;
    dview->search.last_string = NULL;
    dview->search.last_found_line = -1;
    dview->search.last_accessed_num_line = -1;

    dview->opt.quality = 0;
    dview->opt.strip_trailing_cr = 0;
    dview->opt.ignore_tab_expansion = 0;
    dview->opt.ignore_space_change = 0;
    dview->opt.ignore_all_space = 0;
    dview->opt.ignore_case = 0;

    dview_compute_areas (dview);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_fini (WDiff * dview)
{
    if (dview->dsrc != DATA_SRC_MEM)
    {
        f_close (dview->f[DIFF_RIGHT]);
        f_close (dview->f[DIFF_LEFT]);
    }

    if (dview->converter != str_cnv_from_term)
        str_close_conv (dview->converter);

    destroy_hdiff (dview);
    if (dview->a[DIFF_LEFT] != NULL)
    {
        g_array_foreach (dview->a[DIFF_LEFT], DIFFLN, cc_free_elt);
        g_array_free (dview->a[DIFF_LEFT], TRUE);
        dview->a[DIFF_LEFT] = NULL;
    }
    if (dview->a[DIFF_RIGHT] != NULL)
    {
        g_array_foreach (dview->a[DIFF_RIGHT], DIFFLN, cc_free_elt);
        g_array_free (dview->a[DIFF_RIGHT], TRUE);
        dview->a[DIFF_RIGHT] = NULL;
    }

    g_free (dview->label[DIFF_LEFT]);
    g_free (dview->label[DIFF_RIGHT]);
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_display_file (const WDiff * dview, diff_place_t ord, int r, int c, int height, int width)
{
    size_t i, k;
    int j;
    char buf[BUFSIZ];
    FBUF *f = dview->f[ord];
    int skip = dview->skip_cols;
    int display_symbols = dview->display_symbols;
    int display_numbers = dview->display_numbers;
    int show_cr = dview->show_cr;
    int tab_size = 8;
    const DIFFLN *p;
    int nwidth = display_numbers;
    int xwidth;

    xwidth = display_symbols + display_numbers;
    if (dview->tab_size > 0 && dview->tab_size < 9)
        tab_size = dview->tab_size;

    if (xwidth != 0)
    {
        if (xwidth > width && display_symbols)
        {
            xwidth--;
            display_symbols = 0;
        }
        if (xwidth > width && display_numbers)
        {
            xwidth = width;
            display_numbers = width;
        }

        xwidth++;
        c += xwidth;
        width -= xwidth;
        if (width < 0)
            width = 0;
    }

    if ((int) sizeof (buf) <= width || (int) sizeof (buf) <= nwidth)
    {
        /* abnormal, but avoid buffer overflow */
        return -1;
    }

    for (i = dview->skip_rows, j = 0; i < dview->a[ord]->len && j < height; j++, i++)
    {
        int ch, next_ch, col;
        size_t cnt;

        p = (DIFFLN *) & g_array_index (dview->a[ord], DIFFLN, i);
        ch = p->ch;
        tty_setcolor (NORMAL_COLOR);
        if (display_symbols)
        {
            tty_gotoyx (r + j, c - 2);
            tty_print_char (ch);
        }
        if (p->line != 0)
        {
            if (display_numbers)
            {
                tty_gotoyx (r + j, c - xwidth);
                g_snprintf (buf, display_numbers + 1, "%*d", nwidth, p->line);
                tty_print_string (str_fit_to_term (buf, nwidth, J_LEFT_FIT));
            }
            if (ch == ADD_CH)
                tty_setcolor (DFF_ADD_COLOR);
            if (ch == CHG_CH)
                tty_setcolor (DFF_CHG_COLOR);
            if (f == NULL)
            {
                if (i == (size_t) dview->search.last_found_line)
                    tty_setcolor (MARKED_SELECTED_COLOR);
                else if (dview->hdiff != NULL && g_ptr_array_index (dview->hdiff, i) != NULL)
                {
                    char att[BUFSIZ];

                    if (dview->utf8)
                        k = dview_str_utf8_offset_to_pos (p->p, width);
                    else
                        k = width;

                    cvt_mgeta (p->p, p->u.len, buf, k, skip, tab_size, show_cr,
                               g_ptr_array_index (dview->hdiff, i), ord, att);
                    tty_gotoyx (r + j, c);
                    col = 0;

                    for (cnt = 0; cnt < strlen (buf) && col < width; cnt++)
                    {
                        int w;
                        gboolean ch_res;

                        if (dview->utf8)
                        {
                            next_ch = dview_get_utf (buf + cnt, &w, &ch_res);
                            if (w > 1)
                                cnt += w - 1;
                            if (!g_unichar_isprint (next_ch))
                                next_ch = '.';
                        }
                        else
                            next_ch = dview_get_byte (buf + cnt, &ch_res);

                        if (ch_res)
                        {
                            tty_setcolor (att[cnt] ? DFF_CHH_COLOR : DFF_CHG_COLOR);
#ifdef HAVE_CHARSET
                            if (mc_global.utf8_display)
                            {
                                if (!dview->utf8)
                                {
                                    next_ch =
                                        convert_from_8bit_to_utf_c ((unsigned char) next_ch,
                                                                    dview->converter);
                                }
                            }
                            else if (dview->utf8)
                                next_ch = convert_from_utf_to_current_c (next_ch, dview->converter);
                            else
                                next_ch = convert_to_display_c (next_ch);
#endif
                            tty_print_anychar (next_ch);
                            col++;
                        }
                    }
                    continue;
                }

                if (ch == CHG_CH)
                    tty_setcolor (DFF_CHH_COLOR);

                if (dview->utf8)
                    k = dview_str_utf8_offset_to_pos (p->p, width);
                else
                    k = width;
                cvt_mget (p->p, p->u.len, buf, k, skip, tab_size, show_cr);
            }
            else
                cvt_fget (f, p->u.off, buf, width, skip, tab_size, show_cr);
        }
        else
        {
            if (display_numbers)
            {
                tty_gotoyx (r + j, c - xwidth);
                memset (buf, ' ', display_numbers);
                buf[display_numbers] = '\0';
                tty_print_string (buf);
            }
            if (ch == DEL_CH)
                tty_setcolor (DFF_DEL_COLOR);
            if (ch == CHG_CH)
                tty_setcolor (DFF_CHD_COLOR);
            memset (buf, ' ', width);
            buf[width] = '\0';
        }
        tty_gotoyx (r + j, c);
        /* tty_print_nstring (buf, width); */
        col = 0;
        for (cnt = 0; cnt < strlen (buf) && col < width; cnt++)
        {
            int w;
            gboolean ch_res;

            if (dview->utf8)
            {
                next_ch = dview_get_utf (buf + cnt, &w, &ch_res);
                if (w > 1)
                    cnt += w - 1;
                if (!g_unichar_isprint (next_ch))
                    next_ch = '.';
            }
            else
                next_ch = dview_get_byte (buf + cnt, &ch_res);
            if (ch_res)
            {
#ifdef HAVE_CHARSET
                if (mc_global.utf8_display)
                {
                    if (!dview->utf8)
                    {
                        next_ch =
                            convert_from_8bit_to_utf_c ((unsigned char) next_ch, dview->converter);
                    }
                }
                else if (dview->utf8)
                    next_ch = convert_from_utf_to_current_c (next_ch, dview->converter);
                else
                    next_ch = convert_to_display_c (next_ch);
#endif

                tty_print_anychar (next_ch);
                col++;
            }
        }
    }
    tty_setcolor (NORMAL_COLOR);
    k = width;
    if (width < xwidth - 1)
        k = xwidth - 1;
    memset (buf, ' ', k);
    buf[k] = '\0';
    for (; j < height; j++)
    {
        if (xwidth != 0)
        {
            tty_gotoyx (r + j, c - xwidth);
            /* tty_print_nstring (buf, xwidth - 1); */
            tty_print_string (str_fit_to_term (buf, xwidth - 1, J_LEFT_FIT));
        }
        tty_gotoyx (r + j, c);
        /* tty_print_nstring (buf, width); */
        tty_print_string (str_fit_to_term (buf, width, J_LEFT_FIT));
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_status (const WDiff * dview, diff_place_t ord, int width, int c)
{
    const char *buf;
    int filename_width;
    int linenum, lineofs;
    vfs_path_t *vpath;
    char *path;

    tty_setcolor (STATUSBAR_COLOR);

    tty_gotoyx (0, c);
    get_line_numbers (dview->a[ord], dview->skip_rows, &linenum, &lineofs);

    filename_width = width - 24;
    if (filename_width < 8)
        filename_width = 8;

    vpath = vfs_path_from_str (dview->label[ord]);
    path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    vfs_path_free (vpath);
    buf = str_term_trim (path, filename_width);
    if (ord == DIFF_LEFT)
        tty_printf ("%s%-*s %6d+%-4d Col %-4d ", dview->merged[ord] ? "* " : "  ", filename_width,
                    buf, linenum, lineofs, dview->skip_cols);
    else
        tty_printf ("%s%-*s %6d+%-4d Dif %-4d ", dview->merged[ord] ? "* " : "  ", filename_width,
                    buf, linenum, lineofs, dview->ndiff);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_redo (WDiff * dview)
{
    if (dview->display_numbers)
    {
        int old;

        old = dview->display_numbers;
        dview->display_numbers = calc_nwidth ((const GArray **) dview->a);
        dview->new_frame = (old != dview->display_numbers);
    }
    dview_reread (dview);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_update (WDiff * dview)
{
    int height = dview->height;
    int width1;
    int width2;
    int last;

    last = dview->a[DIFF_LEFT]->len - 1;

    if (dview->skip_rows > last)
        dview->skip_rows = dview->search.last_accessed_num_line = last;
    if (dview->skip_rows < 0)
        dview->skip_rows = dview->search.last_accessed_num_line = 0;
    if (dview->skip_cols < 0)
        dview->skip_cols = 0;

    if (height < 2)
        return;

    width1 = dview->half1 + dview->bias;
    width2 = dview->half2 - dview->bias;
    if (dview->full)
    {
        width1 = COLS;
        width2 = 0;
    }

    if (dview->new_frame)
    {
        int xwidth;

        tty_setcolor (NORMAL_COLOR);
        xwidth = dview->display_symbols + dview->display_numbers;
        if (width1 > 1)
            tty_draw_box (1, 0, height, width1, FALSE);
        if (width2 > 1)
            tty_draw_box (1, width1, height, width2, FALSE);

        if (xwidth != 0)
        {
            xwidth++;
            if (xwidth < width1 - 1)
            {
                tty_gotoyx (1, xwidth);
                tty_print_alt_char (mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE], FALSE);
                tty_gotoyx (height, xwidth);
                tty_print_alt_char (mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE], FALSE);
                tty_draw_vline (2, xwidth, mc_tty_frm[MC_TTY_FRM_VERT], height - 2);
            }
            if (xwidth < width2 - 1)
            {
                tty_gotoyx (1, width1 + xwidth);
                tty_print_alt_char (mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE], FALSE);
                tty_gotoyx (height, width1 + xwidth);
                tty_print_alt_char (mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE], FALSE);
                tty_draw_vline (2, width1 + xwidth, mc_tty_frm[MC_TTY_FRM_VERT], height - 2);
            }
        }
        dview->new_frame = 0;
    }

    if (width1 > 2)
    {
        dview_status (dview, dview->ord, width1, 0);
        dview_display_file (dview, dview->ord, 2, 1, height - 2, width1 - 2);
    }
    if (width2 > 2)
    {
        dview_status (dview, dview->ord ^ 1, width2, width1);
        dview_display_file (dview, dview->ord ^ 1, 2, width1 + 1, height - 2, width2 - 2);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_edit (WDiff * dview, diff_place_t ord)
{
    WDialog *h;
    gboolean h_modal;
    int linenum, lineofs;

    if (dview->dsrc == DATA_SRC_TMP)
    {
        error_dialog (_("Edit"), _("Edit is disabled"));
        return;
    }

    h = WIDGET (dview)->owner;
    h_modal = h->modal;

    get_line_numbers (dview->a[ord], dview->skip_rows, &linenum, &lineofs);
    h->modal = TRUE;            /* not allow edit file in several editors */
    {
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_from_str (dview->file[ord]);
        do_edit_at_line (tmp_vpath, use_internal_edit, linenum);
        vfs_path_free (tmp_vpath);
    }
    h->modal = h_modal;
    dview_redo (dview);
    dview_update (dview);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_goto_cmd (WDiff * dview, diff_place_t ord)
{
    /* *INDENT-OFF* */
    static const char *title[2] = {
        N_("Goto line (left)"),
        N_("Goto line (right)")
    };
    /* *INDENT-ON* */
    static char prev[256];
    /* XXX some statics here, to be remembered between runs */

    int newline;
    char *input;

    input =
        input_dialog (_(title[ord]), _("Enter line:"), MC_HISTORY_YDIFF_GOTO_LINE, prev,
                      INPUT_COMPLETE_NONE);
    if (input != NULL)
    {
        const char *s = input;

        if (scan_deci (&s, &newline) == 0 && *s == '\0')
        {
            size_t i = 0;

            if (newline > 0)
            {
                const DIFFLN *p;

                for (; i < dview->a[ord]->len; i++)
                {
                    p = &g_array_index (dview->a[ord], DIFFLN, i);
                    if (p->line == newline)
                        break;
                }
            }
            dview->skip_rows = dview->search.last_accessed_num_line = (ssize_t) i;
            g_snprintf (prev, sizeof (prev), "%d", newline);
        }
        g_free (input);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_labels (WDiff * dview)
{
    Widget *d;
    WDialog *h;
    WButtonBar *b;

    d = WIDGET (dview);
    h = d->owner;
    b = find_buttonbar (h);

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), diff_map, d);
    buttonbar_set_label (b, 2, Q_ ("ButtonBar|Save"), diff_map, d);
    buttonbar_set_label (b, 4, Q_ ("ButtonBar|Edit"), diff_map, d);
    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Merge"), diff_map, d);
    buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), diff_map, d);
    buttonbar_set_label (b, 9, Q_ ("ButtonBar|Options"), diff_map, d);
    buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), diff_map, d);
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_event (Gpm_Event * event, void *data)
{
    WDiff *dview = (WDiff *) data;

    if (!mouse_global_in_widget (event, data))
        return MOU_UNHANDLED;

    /* We are not interested in release events */
    if ((event->type & (GPM_DOWN | GPM_DRAG)) == 0)
        return MOU_NORMAL;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) != 0 && (event->type & GPM_DOWN) != 0)
    {
        dview->skip_rows -= 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
        dview_update (dview);
    }
    else if ((event->buttons & GPM_B_DOWN) != 0 && (event->type & GPM_DOWN) != 0)
    {
        dview->skip_rows += 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
        dview_update (dview);
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dview_save (WDiff * dview)
{
    gboolean res = TRUE;

    if (dview->merged[DIFF_LEFT])
    {
        res = mc_util_unlink_backup_if_possible (dview->file[DIFF_LEFT], "~~~");
        dview->merged[DIFF_LEFT] = !res;
    }
    if (dview->merged[DIFF_RIGHT])
    {
        res = mc_util_unlink_backup_if_possible (dview->file[DIFF_RIGHT], "~~~");
        dview->merged[DIFF_RIGHT] = !res;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_do_save (WDiff * dview)
{
    (void) dview_save (dview);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_save_options (WDiff * dview)
{
    mc_config_set_bool (mc_main_config, "DiffView", "show_symbols",
                        dview->display_symbols != 0 ? TRUE : FALSE);
    mc_config_set_bool (mc_main_config, "DiffView", "show_numbers",
                        dview->display_numbers != 0 ? TRUE : FALSE);
    mc_config_set_int (mc_main_config, "DiffView", "tab_size", dview->tab_size);

    mc_config_set_int (mc_main_config, "DiffView", "diff_quality", dview->opt.quality);

    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_tws",
                        dview->opt.strip_trailing_cr);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_all_space",
                        dview->opt.ignore_all_space);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_space_change",
                        dview->opt.ignore_space_change);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_tab_expansion",
                        dview->opt.ignore_tab_expansion);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_case", dview->opt.ignore_case);
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_load_options (WDiff * dview)
{
    gboolean show_numbers, show_symbols;
    int tab_size;

    show_symbols = mc_config_get_bool (mc_main_config, "DiffView", "show_symbols", FALSE);
    if (show_symbols)
        dview->display_symbols = 1;
    show_numbers = mc_config_get_bool (mc_main_config, "DiffView", "show_numbers", FALSE);
    if (show_numbers)
        dview->display_numbers = calc_nwidth ((const GArray ** const) dview->a);
    tab_size = mc_config_get_int (mc_main_config, "DiffView", "tab_size", 8);
    if (tab_size > 0 && tab_size < 9)
        dview->tab_size = tab_size;
    else
        dview->tab_size = 8;

    dview->opt.quality = mc_config_get_int (mc_main_config, "DiffView", "diff_quality", 0);

    dview->opt.strip_trailing_cr =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_tws", FALSE);
    dview->opt.ignore_all_space =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_all_space", FALSE);
    dview->opt.ignore_space_change =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_space_change", FALSE);
    dview->opt.ignore_tab_expansion =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_tab_expansion", FALSE);
    dview->opt.ignore_case =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_case", FALSE);

    dview->new_frame = 1;
}

/* --------------------------------------------------------------------------------------------- */

/*
 * Check if it's OK to close the diff viewer.  If there are unsaved changes,
 * ask user.
 */
static gboolean
dview_ok_to_exit (WDiff * dview)
{
    gboolean res = TRUE;
    int act;

    if (!dview->merged[DIFF_LEFT] && !dview->merged[DIFF_RIGHT])
        return res;

    act = query_dialog (_("Quit"), !mc_global.midnight_shutdown ?
                        _("File(s) was modified. Save with exit?") :
                        _("Midnight Commander is being shut down.\nSave modified file(s)?"),
                        D_NORMAL, 2, _("&Yes"), _("&No"));

    /* Esc is No */
    if (mc_global.midnight_shutdown || (act == -1))
        act = 1;

    switch (act)
    {
    case -1:                   /* Esc */
        res = FALSE;
        break;
    case 0:                    /* Yes */
        (void) dview_save (dview);
        res = TRUE;
        break;
    case 1:                    /* No */
        if (mc_util_restore_from_backup_if_possible (dview->file[DIFF_LEFT], "~~~"))
            res = mc_util_unlink_backup_if_possible (dview->file[DIFF_LEFT], "~~~");
        if (mc_util_restore_from_backup_if_possible (dview->file[DIFF_RIGHT], "~~~"))
            res = mc_util_unlink_backup_if_possible (dview->file[DIFF_RIGHT], "~~~");
        /* fall through */
    default:
        res = TRUE;
        break;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_execute_cmd (WDiff * dview, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    switch (command)
    {
    case CK_ShowSymbols:
        dview->display_symbols ^= 1;
        dview->new_frame = 1;
        break;
    case CK_ShowNumbers:
        dview->display_numbers ^= calc_nwidth ((const GArray ** const) dview->a);
        dview->new_frame = 1;
        break;
    case CK_SplitFull:
        dview->full ^= 1;
        dview->new_frame = 1;
        break;
    case CK_SplitEqual:
        if (!dview->full)
        {
            dview->bias = 0;
            dview->new_frame = 1;
        }
        break;
    case CK_SplitMore:
        if (!dview->full)
        {
            dview_compute_split (dview, 1);
            dview->new_frame = 1;
        }
        break;

    case CK_SplitLess:
        if (!dview->full)
        {
            dview_compute_split (dview, -1);
            dview->new_frame = 1;
        }
        break;
    case CK_Tab2:
        dview->tab_size = 2;
        break;
    case CK_Tab3:
        dview->tab_size = 3;
        break;
    case CK_Tab4:
        dview->tab_size = 4;
        break;
    case CK_Tab8:
        dview->tab_size = 8;
        break;
    case CK_Swap:
        dview->ord ^= 1;
        break;
    case CK_Redo:
        dview_redo (dview);
        break;
    case CK_HunkNext:
        dview->skip_rows = dview->search.last_accessed_num_line =
            find_next_hunk (dview->a[DIFF_LEFT], dview->skip_rows);
        break;
    case CK_HunkPrev:
        dview->skip_rows = dview->search.last_accessed_num_line =
            find_prev_hunk (dview->a[DIFF_LEFT], dview->skip_rows);
        break;
    case CK_Goto:
        dview_goto_cmd (dview, TRUE);
        break;
    case CK_Edit:
        dview_edit (dview, dview->ord);
        break;
    case CK_Merge:
        do_merge_hunk (dview, FROM_LEFT_TO_RIGHT);
        dview_redo (dview);
        break;
    case CK_MergeOther:
        do_merge_hunk (dview, FROM_RIGHT_TO_LEFT);
        dview_redo (dview);
        break;
    case CK_EditOther:
        dview_edit (dview, dview->ord ^ 1);
        break;
    case CK_Search:
        dview_search_cmd (dview);
        break;
    case CK_SearchContinue:
        dview_continue_search_cmd (dview);
        break;
    case CK_Top:
        dview->skip_rows = dview->search.last_accessed_num_line = 0;
        break;
    case CK_Bottom:
        dview->skip_rows = dview->search.last_accessed_num_line = dview->a[DIFF_LEFT]->len - 1;
        break;
    case CK_Up:
        if (dview->skip_rows > 0)
        {
            dview->skip_rows--;
            dview->search.last_accessed_num_line = dview->skip_rows;
        }
        break;
    case CK_Down:
        dview->skip_rows++;
        dview->search.last_accessed_num_line = dview->skip_rows;
        break;
    case CK_PageDown:
        if (dview->height > 2)
        {
            dview->skip_rows += dview->height - 2;
            dview->search.last_accessed_num_line = dview->skip_rows;
        }
        break;
    case CK_PageUp:
        if (dview->height > 2)
        {
            dview->skip_rows -= dview->height - 2;
            dview->search.last_accessed_num_line = dview->skip_rows;
        }
        break;
    case CK_Left:
        dview->skip_cols--;
        break;
    case CK_Right:
        dview->skip_cols++;
        break;
    case CK_LeftQuick:
        dview->skip_cols -= 8;
        break;
    case CK_RightQuick:
        dview->skip_cols += 8;
        break;
    case CK_Home:
        dview->skip_cols = 0;
        break;
    case CK_Shell:
        view_other_cmd ();
        break;
    case CK_Quit:
        dview->view_quit = 1;
        break;
    case CK_Save:
        dview_do_save (dview);
        break;
    case CK_Options:
        dview_diff_options (dview);
        break;
#ifdef HAVE_CHARSET
    case CK_SelectCodepage:
        dview_select_encoding (dview);
        break;
#endif
    case CK_Cancel:
        /* don't close diffviewer due to SIGINT */
        break;
    default:
        res = MSG_NOT_HANDLED;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_handle_key (WDiff * dview, int key)
{
    unsigned long command;

#ifdef HAVE_CHARSET
    key = convert_from_input_c (key);
#endif

    command = keybind_lookup_keymap_command (diff_map, key);
    if ((command != CK_IgnoreKey) && (dview_execute_cmd (dview, command) == MSG_HANDLED))
        return MSG_HANDLED;

    /* Key not used */
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDiff *dview = (WDiff *) w;
    WDialog *h = w->owner;
    cb_ret_t i;

    switch (msg)
    {
    case MSG_INIT:
        dview_labels (dview);
        dview_load_options (dview);
        dview_update (dview);
        return MSG_HANDLED;

    case MSG_DRAW:
        dview->new_frame = 1;
        dview_update (dview);
        return MSG_HANDLED;

    case MSG_KEY:
        i = dview_handle_key (dview, parm);
        if (dview->view_quit)
            dlg_stop (h);
        else
            dview_update (dview);
        return i;

    case MSG_ACTION:
        i = dview_execute_cmd (dview, parm);
        if (dview->view_quit)
            dlg_stop (h);
        else
            dview_update (dview);
        return i;

    case MSG_DESTROY:
        dview_save_options (dview);
        dview_fini (dview);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_adjust_size (WDialog * h)
{
    WDiff *dview;
    WButtonBar *bar;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    dview = (WDiff *) find_widget_type (h, dview_callback);
    bar = find_buttonbar (h);
    widget_set_size (WIDGET (dview), 0, 0, LINES - 1, COLS);
    widget_set_size (WIDGET (bar), LINES - 1, 0, 1, COLS);

    dview_compute_areas (dview);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_dialog_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDiff *dview = (WDiff *) data;
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_RESIZE:
        dview_adjust_size (h);
        return MSG_HANDLED;

    case MSG_ACTION:
        /* shortcut */
        if (sender == NULL)
            return dview_execute_cmd (NULL, parm);
        /* message from buttonbar */
        if (sender == WIDGET (find_buttonbar (h)))
        {
            if (data != NULL)
                return send_message (data, NULL, MSG_ACTION, parm, NULL);

            dview = (WDiff *) find_widget_type (h, dview_callback);
            return dview_execute_cmd (dview, parm);
        }
        return MSG_NOT_HANDLED;

    case MSG_VALIDATE:
        dview = (WDiff *) find_widget_type (h, dview_callback);
        h->state = DLG_ACTIVE;  /* don't stop the dialog before final decision */
        if (dview_ok_to_exit (dview))
            h->state = DLG_CLOSED;
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
dview_get_title (const WDialog * h, size_t len)
{
    const WDiff *dview;
    const char *modified = " (*) ";
    const char *notmodified = "     ";
    size_t len1;
    GString *title;

    dview = (const WDiff *) find_widget_type (h, dview_callback);
    len1 = (len - str_term_width1 (_("Diff:")) - strlen (modified) - 3) / 2;

    title = g_string_sized_new (len);
    g_string_append (title, _("Diff:"));
    g_string_append (title, dview->merged[DIFF_LEFT] ? modified : notmodified);
    g_string_append (title, str_term_trim (dview->label[DIFF_LEFT], len1));
    g_string_append (title, " | ");
    g_string_append (title, dview->merged[DIFF_RIGHT] ? modified : notmodified);
    g_string_append (title, str_term_trim (dview->label[DIFF_RIGHT], len1));

    return g_string_free (title, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static int
diff_view (const char *file1, const char *file2, const char *label1, const char *label2)
{
    int error;
    WDiff *dview;
    Widget *w;
    WDialog *dview_dlg;

    /* Create dialog and widgets, put them on the dialog */
    dview_dlg =
        create_dlg (FALSE, 0, 0, LINES, COLS, NULL, dview_dialog_callback, NULL,
                    "[Diff Viewer]", NULL, DLG_WANT_TAB);

    dview = g_new0 (WDiff, 1);
    w = WIDGET (dview);
    init_widget (w, 0, 0, LINES - 1, COLS, dview_callback, dview_event);
    widget_want_cursor (w, FALSE);

    add_widget (dview_dlg, dview);
    add_widget (dview_dlg, buttonbar_new (TRUE));

    dview_dlg->get_title = dview_get_title;

    error = dview_init (dview, "-a", file1, file2, label1, label2, DATA_SRC_MEM);       /* XXX binary diff? */

    /* Please note that if you add another widget,
     * you have to modify dview_adjust_size to
     * be aware of it
     */
    if (error == 0)
        run_dlg (dview_dlg);

    if ((error != 0) || (dview_dlg->state == DLG_CLOSED))
        destroy_dlg (dview_dlg);

    return error == 0 ? 1 : 0;
}

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#define GET_FILE_AND_STAMP(n) \
do \
{ \
    use_copy##n = 0; \
    real_file##n = file##n; \
    if (!vfs_file_is_local (file##n)) \
    { \
        real_file##n = mc_getlocalcopy (file##n); \
        if (real_file##n != NULL) \
        { \
            use_copy##n = 1; \
            if (mc_stat (real_file##n, &st##n) != 0) \
                use_copy##n = -1; \
        } \
    } \
} \
while (0)

#define UNGET_FILE(n) \
do \
{ \
    if (use_copy##n) \
    { \
        int changed = 0; \
        if (use_copy##n > 0) \
        { \
            time_t mtime; \
            mtime = st##n.st_mtime; \
            if (mc_stat (real_file##n, &st##n) == 0) \
                changed = (mtime != st##n.st_mtime); \
        } \
        mc_ungetlocalcopy (file##n, real_file##n, changed); \
        vfs_path_free (real_file##n); \
    } \
} \
while (0)

gboolean
dview_diff_cmd (const void *f0, const void *f1)
{
    int rv = 0;
    vfs_path_t *file0 = NULL;
    vfs_path_t *file1 = NULL;
    gboolean is_dir0 = FALSE;
    gboolean is_dir1 = FALSE;

    switch (mc_global.mc_run_mode)
    {
    case MC_RUN_FULL:
        {
            /* run from panels */
            const WPanel *panel0 = (const WPanel *) f0;
            const WPanel *panel1 = (const WPanel *) f1;

            file0 = vfs_path_append_new (panel0->cwd_vpath, selection (panel0)->fname, NULL);
            is_dir0 = S_ISDIR (selection (panel0)->st.st_mode);
            if (is_dir0)
            {
                message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"),
                         path_trunc (selection (panel0)->fname, 30));
                goto ret;
            }

            file1 = vfs_path_append_new (panel1->cwd_vpath, selection (panel1)->fname, NULL);
            is_dir1 = S_ISDIR (selection (panel1)->st.st_mode);
            if (is_dir1)
            {
                message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"),
                         path_trunc (selection (panel1)->fname, 30));
                goto ret;
            }
            break;
        }

    case MC_RUN_DIFFVIEWER:
        {
            /* run from command line */
            const char *p0 = (const char *) f0;
            const char *p1 = (const char *) f1;
            struct stat st;

            file0 = vfs_path_from_str (p0);
            if (mc_stat (file0, &st) == 0)
            {
                is_dir0 = S_ISDIR (st.st_mode);
                if (is_dir0)
                {
                    message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"), path_trunc (p0, 30));
                    goto ret;
                }
            }
            else
            {
                message (D_ERROR, MSG_ERROR, _("Cannot stat \"%s\"\n%s"),
                         path_trunc (p0, 30), unix_error_string (errno));
                goto ret;
            }

            file1 = vfs_path_from_str (p1);
            if (mc_stat (file1, &st) == 0)
            {
                is_dir1 = S_ISDIR (st.st_mode);
                if (is_dir1)
                {
                    message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"), path_trunc (p1, 30));
                    goto ret;
                }
            }
            else
            {
                message (D_ERROR, MSG_ERROR, _("Cannot stat \"%s\"\n%s"),
                         path_trunc (p1, 30), unix_error_string (errno));
                goto ret;
            }
            break;
        }

    default:
        /* this should not happaned */
        message (D_ERROR, MSG_ERROR, _("Diff viewer: invalid mode"));
        return FALSE;
    }

    if (rv == 0)
    {
        rv = -1;
        if (file0 != NULL && file1 != NULL)
        {
            int use_copy0;
            int use_copy1;
            struct stat st0;
            struct stat st1;
            vfs_path_t *real_file0;
            vfs_path_t *real_file1;

            GET_FILE_AND_STAMP (0);
            GET_FILE_AND_STAMP (1);
            if (real_file0 != NULL && real_file1 != NULL)
            {
                char *real_file0_str, *real_file1_str;
                char *file0_str, *file1_str;

                real_file0_str = vfs_path_to_str (real_file0);
                real_file1_str = vfs_path_to_str (real_file1);
                file0_str = vfs_path_to_str (file0);
                file1_str = vfs_path_to_str (file1);
                rv = diff_view (real_file0_str, real_file1_str, file0_str, file1_str);
                g_free (real_file0_str);
                g_free (real_file1_str);
                g_free (file0_str);
                g_free (file1_str);
            }
            UNGET_FILE (1);
            UNGET_FILE (0);
        }
    }

    if (rv == 0)
        message (D_ERROR, MSG_ERROR, _("Two files are needed to compare"));

  ret:
    vfs_path_free (file1);
    vfs_path_free (file0);

    return (rv != 0);
}

#undef GET_FILE_AND_STAMP
#undef UNGET_FILE

/* --------------------------------------------------------------------------------------------- */
