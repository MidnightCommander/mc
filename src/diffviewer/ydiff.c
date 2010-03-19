/*
   Copyright (C) 2007, 2010  Free Software Foundation, Inc.
   Written by:
   2007 Daniel Borca <dborca@yahoo.com>

   2010 Slava Zanko <slavazanko@gmail.com>
   2010 Andrew Borodin <aborodin@vmail.ru>
   2010 Ilia Maslakov <il.smind@gmail.com>


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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "src/cmddef.h"
#include "src/keybind.h"
#include "lib/global.h"
#include "lib/tty/tty.h"
#include "src/cmd.h"
#include "src/dialog.h"
#include "src/widget.h"
#include "lib/tty/color.h"
#include "src/help.h"
#include "lib/tty/key.h"
#include "src/wtools.h"
#include "src/charsets.h"
#include "src/history.h"
#include "src/panel.h"                  /* Needed for current_panel and other_panel */
#include "src/layout.h"                 /* Needed for get_current_index and get_other_panel */
#include "lib/skin.h"                   /* EDITOR_NORMAL_COLOR */
#include "lib/vfs/mc-vfs/vfs.h"         /* mc_opendir, mc_readdir, mc_closedir, */
                                        /* mc_open, mc_close, mc_read, mc_stat  */
#include "src/main.h"                   /* mc_run_mode */
#include "ydiff.h"

#ifdef USE_DIFF_VIEW

/*** global variables ****************************************************************************/

const global_keymap_t *diff_map;

/*** file scope macro definitions ****************************************************************/

#define g_array_foreach(a, TP, cbf) \
do { \
    size_t g_array_foreach_i;\
    TP *g_array_foreach_var=NULL; \
    for (g_array_foreach_i=0;g_array_foreach_i < a->len; g_array_foreach_i++) \
    { \
        g_array_foreach_var = &g_array_index(a,TP,g_array_foreach_i); \
        (*cbf) (g_array_foreach_var); \
    } \
} while (0)

#define FILE_READ_BUF	4096
#define FILE_FLAG_TEMP	(1 << 0)

#define OPTX 50
#define OPTY 16

#define SEARCH_DLG_WIDTH  58
#define SEARCH_DLG_HEIGHT 14

#define ADD_CH		'+'
#define DEL_CH		'-'
#define CHG_CH		'*'
#define EQU_CH		' '

#define HDIFF_ENABLE	1
#define HDIFF_MINCTX	5
#define HDIFF_DEPTH	10

#define TAB_SKIP(ts, pos)	((ts) - (pos) % (ts))

#define error_dialog(h, s) query_dialog(h, s, D_ERROR, 1, _("&Dismiss"))

#define FILE_DIRTY(fs)	\
    do {		\
	(fs)->pos = 0;	\
	(fs)->len = 0;	\
    } while (0)

#define IS_WHOLE_OR_DONT_CARE()							\
    (!whole || (								\
     (i == 0 || strchr(wholechars, haystack[i - 1]) == NULL) &&			\
     (i + nlen == hlen || strchr(wholechars, haystack[i + nlen]) == NULL)	\
    ))

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int fd;
    int pos;
    int len;
    char *buf;
    int flags;
    void *data;
} FBUF;

typedef struct
{
    int a[2][2];
    int cmd;
} DIFFCMD;

typedef int (*DFUNC) (void *ctx, int ch, int line, off_t off, size_t sz, const char *str);

typedef struct
{
    int off;
    int len;
} BRACKET[2];

typedef int PAIR[2];

typedef enum
{
    DATA_SRC_MEM = 0,
    DATA_SRC_TMP = 1,
    DATA_SRC_ORG = 2
} DSRC;

typedef struct
{
    int ch;
    int line;
    union
    {
        off_t off;
        size_t len;
    } u;
    void *p;
} DIFFLN;

typedef struct
{
    FBUF *f;
    GArray *a;
    DSRC dsrc;
} PRINTER_CTX;

typedef struct
{
    Widget widget;

    const char *args;           /* Args passed to diff */
    const char *file[2];        /* filenames */
    const char *label[2];
    FBUF *f[2];
    GArray *a[2];
    GPtrArray *hdiff;
    int ndiff;                  /* number of hunks */
    DSRC dsrc;                  /* data source: memory or temporary file */

    int view_quit:1;            /* Quit flag */

    int height;
    int half1;
    int half2;
    int width1;
    int width2;
    int bias;
    int new_frame;
    int skip_rows;
    int skip_cols;
    int display_symbols;
    int display_numbers;
    int show_cr;
    int tab_size;
    int ord;
    int full;
    ssize_t last_found;

    struct
    {
        int quality;
        gboolean strip_trailing_cr;
        gboolean ignore_tab_expansion;
        gboolean ignore_space_change;
        gboolean ignore_all_space;
        gboolean ignore_case;
    } opt;
} WDiff;

/*** file scope variables ************************************************************************/

static const char *wholechars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* buffered I/O ************************************************************* */

/**
 * Try to open a temporary file.
 *
 * \param[out] name address of a pointer to store the temporary name
 *
 * \return file descriptor on success, negative on error
 *
 * \note the name is not altered if this function fails
 */

static int
open_temp (void **name)
{
    int fd;
    char *diff_file_name = NULL;

    fd = mc_mkstemps (&diff_file_name, "mcdiff", NULL);
    if (fd == -1)
    {
        message (D_ERROR, MSG_ERROR,
        _(" Cannot create temporary diff file \n %s "),
        unix_error_string (errno));
        return -1;
    }
    *name = diff_file_name;
    return fd;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Alocate file structure and associate file descriptor to it.
 *
 * \param fd file descriptor
 *
 * \return file structure
 */
static FBUF *
f_dopen (int fd)
{
    FBUF *fs;

    if (fd < 0)
    {
        return NULL;
    }

    fs = g_malloc (sizeof (FBUF));
    if (fs == NULL)
    {
        return NULL;
    }

    fs->buf = g_malloc (FILE_READ_BUF);
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
 * \param fs file structure
 *
 * \return 0 on success, non-zero on error
 */
static int
f_free (FBUF * fs)
{
    int rv = 0;
    if (fs->flags & FILE_FLAG_TEMP)
    {
        rv = unlink (fs->data);
        free (fs->data);
    }
    g_free (fs->buf);
    g_free (fs);
    return rv;
}


/* --------------------------------------------------------------------------------------------- */

/**
 * Open a binary temporary file in R/W mode.
 *
 * \return file structure
 *
 * \note the file will be deleted when closed
 */
static FBUF *
f_temp (void)
{
    int fd;
    FBUF *fs;

    fs = f_dopen (0);
    if (fs == NULL)
    {
        return NULL;
    }

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
 * \param filename file name
 * \param flags open mode, a combination of O_RDONLY, O_WRONLY, O_RDWR
 *
 * \return file structure
 */
static FBUF *
f_open (const char *filename, int flags)
{
    int fd;
    FBUF *fs;

    fs = f_dopen (0);
    if (fs == NULL)
    {
        return NULL;
    }

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
 *
 * \param buf destination buffer
 * \param size size of buffer
 * \param fs file structure
 *
 * \return number of bytes read
 *
 * \note does not stop on null-byte
 * \note buf will not be null-terminated
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
            {
                stop = 1;
            }
        }
        fs->pos = i;

        if (j == size || stop)
        {
            break;
        }

        fs->pos = 0;
        fs->len = read (fs->fd, fs->buf, FILE_READ_BUF);
    }
    while (fs->len > 0);

    return j;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Seek into file.
 *
 * \param fs file structure
 * \param off offset
 * \param whence seek directive: SEEK_SET, SEEK_CUR or SEEK_END
 *
 * \return position in file, starting from begginning
 *
 * \note avoids thrashing read cache when possible
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
    {
        FILE_DIRTY (fs);
    }
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Seek to the beginning of file, thrashing read cache.
 *
 * \param fs file structure
 *
 * \return 0 if success, non-zero on error
 */
static off_t
f_reset (FBUF * fs)
{
    off_t rv = lseek (fs->fd, 0, SEEK_SET);
    if (rv != -1)
    {
        FILE_DIRTY (fs);
    }
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write bytes to file.
 *
 * \param fs file structure
 * \param buf source buffer
 * \param size size of buffer
 *
 * \return number of written bytes, -1 on error
 *
 * \note thrashes read cache
 */
static ssize_t
f_write (FBUF * fs, const char *buf, size_t size)
{
    ssize_t rv = write (fs->fd, buf, size);
    if (rv >= 0)
    {
        FILE_DIRTY (fs);
    }
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Truncate file to the current position.
 *
 * \param fs file structure
 *
 * \return current file size on success, negative on error
 *
 * \note thrashes read cache
 */
static off_t
f_trunc (FBUF * fs)
{
    off_t off = lseek (fs->fd, 0, SEEK_CUR);
    if (off != -1)
    {
        int rv = ftruncate (fs->fd, off);
        if (rv != 0)
        {
            off = -1;
        }
        else
        {
            FILE_DIRTY (fs);
        }
    }
    return off;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Close file.
 *
 * \param fs file structure
 *
 * \return 0 on success, non-zero on error
 *
 * \note if this is temporary file, it is deleted
 */
static int
f_close (FBUF * fs)
{
    int rv = close (fs->fd);
    f_free (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Create pipe stream to process.
 *
 * \param cmd shell command line
 * \param flags open mode, either O_RDONLY or O_WRONLY
 *
 * \return file structure
 */
static FBUF *
p_open (const char *cmd, int flags)
{
    FILE *f;
    FBUF *fs;
    const char *type = NULL;

    if (flags == O_RDONLY)
    {
        type = "r";
    }
    if (flags == O_WRONLY)
    {
        type = "w";
    }

    if (type == NULL)
    {
        return NULL;
    }

    fs = f_dopen (0);
    if (fs == NULL)
    {
        return NULL;
    }

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
 * \param fs structure
 *
 * \return 0 on success, non-zero on error
 */
static int
p_close (FBUF * fs)
{
    int rv = pclose (fs->data);
    f_free (fs);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
/* diff parse *************************************************************** */

/**
 * Read decimal number from string.
 *
 * \param[in,out] str string to parse
 * \param[out] n extracted number
 *
 * \return 0 if success, otherwise non-zero
 */
static int
scan_deci (const char **str, int *n)
{
    const char *p = *str;
    char *q;
    errno = 0;
    *n = strtol (p, &q, 10);
    if (errno || p == q)
    {
        return -1;
    }
    *str = q;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Parse line for diff statement.
 *
 * \param p string to parse
 * \param ops list of diff statements
 *
 * \return 0 if success, otherwise non-zero
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
    {
        return -1;
    }
    f2 = f1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (scan_deci (&p, &f2) != 0 || f2 < f1)
        {
            return -1;
        }
        range = 1;
    }

    cmd = *p++;
    if (cmd == 'a')
    {
        if (range)
        {
            return -1;
        }
    }
    else if (cmd != 'c' && cmd != 'd')
    {
        return -1;
    }

    if (scan_deci (&p, &t1) != 0 || t1 < 0)
    {
        return -1;
    }
    t2 = t1;
    range = 0;
    if (*p == ',')
    {
        p++;
        if (scan_deci (&p, &t2) != 0 || t2 < t1)
        {
            return -1;
        }
        range = 1;
    }

    if (cmd == 'd')
    {
        if (range)
        {
            return -1;
        }
    }

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
 * \param f stream to read from
 * \param ops list of diff statements to fill
 *
 * \return positive number indicating number of hunks, otherwise negative
 */
static int
scan_diff (FBUF * f, GArray * ops)
{
    int sz;
    char buf[BUFSIZ];

    while ((sz = f_gets (buf, sizeof (buf) - 1, f)))
    {
        if (isdigit (buf[0]))
        {
            if (buf[sz - 1] != '\n')
            {
                return -1;
            }
            buf[sz] = '\0';
            if (scan_line (buf, ops) != 0)
            {
                return -1;
            }
            continue;
        }
        while (buf[sz - 1] != '\n' && (sz = f_gets (buf, sizeof (buf), f)))
        {
        }
    }

    return ops->len;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Invoke diff and extract diff statements.
 *
 * \param args extra arguments to be passed to diff
 * \param extra more arguments to be passed to diff
 * \param file1 first file to compare
 * \param file2 second file to compare
 * \param ops list of diff statements to fill
 *
 * \return positive number indicating number of hunks, otherwise negative
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

    cmd =
        malloc (14 + strlen (args) + strlen (extra) + strlen (opt) + strlen (file1) +
                strlen (file2));
    if (cmd == NULL)
    {
        return -1;
    }
    sprintf (cmd, "diff %s %s %s \"%s\" \"%s\"", args, extra, opt, file1, file2);

    f = p_open (cmd, O_RDONLY);
    free (cmd);
    if (f == NULL)
    {
        return -1;
    }

    rv = scan_diff (f, ops);
    code = p_close (f);

    if (rv < 0 || code == -1 || !WIFEXITED (code) || WEXITSTATUS (code) == 2)
    {
        return -1;
    }

    return rv;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Reparse and display file according to diff statements.
 *
 * \param ord 0 if displaying first file, 1 if displaying 2nd file
 * \param filename file name to display
 * \param ops list of diff statements
 * \param printer printf-like function to be used for displaying
 * \param ctx printer context
 *
 * \return 0 if success, otherwise non-zero
 */
static int
dff_reparse (int ord, const char *filename, const GArray * ops, DFUNC printer, void *ctx)
{
    size_t i;
    FBUF *f;
    size_t sz;
    char buf[BUFSIZ];
    int line = 0;
    off_t off = 0;
    const DIFFCMD *op;
    int eff;
    int add_cmd;
    int del_cmd;

    f = f_open (filename, O_RDONLY);
    if (f == NULL)
    {
        return -1;
    }

    ord &= 1;
    eff = ord;

    add_cmd = 'a';
    del_cmd = 'd';
    if (ord)
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
        while (line < n && (sz = f_gets (buf, sizeof (buf), f)))
        {
            line++;
            printer (ctx, EQU_CH, line, off, sz, buf);
            off += sz;
            while (buf[sz - 1] != '\n')
            {
                if (!(sz = f_gets (buf, sizeof (buf), f)))
                {
                    printer (ctx, 0, 0, 0, 1, "\n");
                    break;
                }
                printer (ctx, 0, 0, 0, sz, buf);
                off += sz;
            }
        }
        if (line != n)
        {
            goto err;
        }

        if (op->cmd == add_cmd)
        {
            n = op->T2 - op->T1 + 1;
            while (n)
            {
                printer (ctx, DEL_CH, 0, 0, 1, "\n");
                n--;
            }
        }
        if (op->cmd == del_cmd)
        {
            n = op->F2 - op->F1 + 1;
            while (n && (sz = f_gets (buf, sizeof (buf), f)))
            {
                line++;
                printer (ctx, ADD_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    if (!(sz = f_gets (buf, sizeof (buf), f)))
                    {
                        printer (ctx, 0, 0, 0, 1, "\n");
                        break;
                    }
                    printer (ctx, 0, 0, 0, sz, buf);
                    off += sz;
                }
                n--;
            }
            if (n)
            {
                goto err;
            }
        }
        if (op->cmd == 'c')
        {
            n = op->F2 - op->F1 + 1;
            while (n && (sz = f_gets (buf, sizeof (buf), f)))
            {
                line++;
                printer (ctx, CHG_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    if (!(sz = f_gets (buf, sizeof (buf), f)))
                    {
                        printer (ctx, 0, 0, 0, 1, "\n");
                        break;
                    }
                    printer (ctx, 0, 0, 0, sz, buf);
                    off += sz;
                }
                n--;
            }
            if (n)
            {
                goto err;
            }
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

    while ((sz = f_gets (buf, sizeof (buf), f)))
    {
        line++;
        printer (ctx, EQU_CH, line, off, sz, buf);
        off += sz;
        while (buf[sz - 1] != '\n')
        {
            if (!(sz = f_gets (buf, sizeof (buf), f)))
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
 * \param s first string
 * \param m length of first string
 * \param t second string
 * \param n length of second string
 * \param ret list of offsets for longest common substrings inside each string
 * \param min minimum length of common substrings
 *
 * \return 0 if success, nonzero otherwise
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

    Lprev = g_new0 (int, n + 1);
    Lcurr = g_new0 (int, n + 1);

    if (Lprev == NULL || Lcurr == NULL)
    {
        g_free (Lprev);
        g_free (Lcurr);
        return -1;
    }

    for (i = 0; i < m; i++)
    {
        int *L = Lprev;
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
                int v = Lprev[j] + 1;
                Lcurr[j + 1] = v;
                if (z < v)
                {
                    z = v;
                    g_array_set_size (ret, 0);
                }
                if (z == v && z >= min)
                {
                    int off0 = i - z + 1;
                    int off1 = j - z + 1;
                    size_t k;
                    for (k = 0; k < ret->len; k++)
                    {
                        PAIR *p = (PAIR *) g_array_index (ret, PAIR, k);
                        if ((*p)[0] == off0)
                        {
                            break;
                        }
                        if ((*p)[1] == off1)
                        {
                            break;
                        }
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

    free (Lcurr);
    free (Lprev);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Scan recursively for common substrings and build ranges.
 *
 * \param s first string
 * \param t second string
 * \param bracket current limits for both of the strings
 * \param min minimum length of common substrings
 * \param hdiff list of horizontal diff ranges to fill
 * \param depth recursion depth
 *
 * \return 0 if success, nonzero otherwise
 */
static gboolean
hdiff_multi (const char *s, const char *t, const BRACKET bracket, int min, GArray * hdiff,
             unsigned int depth)
{
    BRACKET p;

    if (depth--)
    {
        GArray *ret;
        BRACKET b;
        int len;
        ret = g_array_new (FALSE, TRUE, sizeof (PAIR));
        if (ret == NULL)
            return FALSE;

        len = lcsubstr (s + bracket[0].off, bracket[0].len,
                        t + bracket[1].off, bracket[1].len, ret, min);
        if (ret->len)
        {
            size_t k = 0;
            const PAIR *data = (const PAIR *) &g_array_index (ret, PAIR, 0);
            const PAIR *data2;

            b[0].off = bracket[0].off;
            b[0].len = (*data)[0];
            b[1].off = bracket[1].off;
            b[1].len = (*data)[1];
            if (!hdiff_multi (s, t, b, min, hdiff, depth))
                return FALSE;

            for (k = 0; k < ret->len - 1; k++)
            {
                data = (const PAIR *) &g_array_index (ret, PAIR, k);
                data2 = (const PAIR *) &g_array_index (ret, PAIR, k + 1);
                b[0].off = bracket[0].off + (*data)[0] + len;
                b[0].len = (*data2)[0] - (*data)[0] - len;
                b[1].off = bracket[1].off + (*data)[1] + len;
                b[1].len = (*data2)[1] - (*data)[1] - len;
                if (!hdiff_multi (s, t, b, min, hdiff, depth))
                    return FALSE;
            }
            data = (const PAIR *) &g_array_index (ret, PAIR, k);
            b[0].off = bracket[0].off + (*data)[0] + len;
            b[0].len = bracket[0].len - (*data)[0] - len;
            b[1].off = bracket[1].off + (*data)[1] + len;
            b[1].len = bracket[1].len - (*data)[1] - len;
            if (!hdiff_multi (s, t, b, min, hdiff, depth))
                return FALSE;

            g_array_free (ret, TRUE);
            return TRUE;
        }
    }

    p[0].off = bracket[0].off;
    p[0].len = bracket[0].len;
    p[1].off = bracket[1].off;
    p[1].len = bracket[1].len;
    g_array_append_val (hdiff, p);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Build list of horizontal diff ranges.
 *
 * \param s first string
 * \param m length of first string
 * \param t second string
 * \param n length of second string
 * \param min minimum length of common substrings
 * \param hdiff list of horizontal diff ranges to fill
 * \param depth recursion depth
 *
 * \return 0 if success, nonzero otherwise
 */
static gboolean
hdiff_scan (const char *s, int m, const char *t, int n, int min, GArray * hdiff, unsigned int depth)
{
    int i;
    BRACKET b;

    /* dumbscan (single horizontal diff) -- does not compress whitespace */

    for (i = 0; i < m && i < n && s[i] == t[i]; i++)
    {
    }
    for (; m > i && n > i && s[m - 1] == t[n - 1]; m--, n--)
    {
    }
    b[0].off = i;
    b[0].len = m - i;
    b[1].off = i;
    b[1].len = n - i;

    /* smartscan (multiple horizontal diff) */

    return hdiff_multi (s, t, b, min, hdiff, depth);
}

/* --------------------------------------------------------------------------------------------- */
/* read line **************************************************************** */

/**
 * Check if character is inside horizontal diff limits.
 *
 * \param k rank of character inside line
 * \param hdiff horizontal diff structure
 * \param ord 0 if reading from first file, 1 if reading from 2nd file
 *
 * \return TRUE if inside hdiff limits, FALSE otherwise
 */
static int
is_inside (int k, GArray * hdiff, int ord)
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
        {
            return 1;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copy `src' to `dst' expanding tabs.
 *
 * \param dst destination buffer
 * \param src source buffer
 * \param srcsize size of src buffer
 * \param base virtual base of this string, needed to calculate tabs
 * \param ts tab size
 *
 * \return new virtual base
 *
 * \note The procedure returns when all bytes are consumed from `src'
 */
static int
cvt_cpy (char *dst, const char *src, size_t srcsize, int base, int ts)
{
    int i;
    for (i = 0; srcsize; i++, src++, dst++, srcsize--)
    {
        *dst = *src;
        if (*src == '\t')
        {
            int j = TAB_SKIP (ts, i + base);
            i += j - 1;
            while (j-- > 0)
            {
                *dst++ = ' ';
            }
            dst--;
        }
    }
    return i + base;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copy `src' to `dst' expanding tabs.
 *
 * \param dst destination buffer
 * \param dstsize size of dst buffer
 * \param[in,out] _src source buffer
 * \param srcsize size of src buffer
 * \param base virtual base of this string, needed to calculate tabs
 * \param ts tab size
 *
 * \return new virtual base
 *
 * \note The procedure returns when all bytes are consumed from `src'
 *       or `dstsize' bytes are written to `dst'
 * \note Upon return, `src' points to the first unwritten character in source
 */
static int
cvt_ncpy (char *dst, int dstsize, const char **_src, size_t srcsize, int base, int ts)
{
    int i;
    const char *src = *_src;
    for (i = 0; i < dstsize && srcsize; i++, src++, dst++, srcsize--)
    {
        *dst = *src;
        if (*src == '\t')
        {
            int j = TAB_SKIP (ts, i + base);
            if (j > dstsize - i)
            {
                j = dstsize - i;
            }
            i += j - 1;
            while (j-- > 0)
            {
                *dst++ = ' ';
            }
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
 * \param src buffer to read from
 * \param srcsize size of src buffer
 * \param dst buffer to read to
 * \param dstsize size of dst buffer, excluding trailing null
 * \param skip number of characters to skip
 * \param ts tab size
 * \param show_cr show trailing carriage return as ^M
 *
 * \return negative on error, otherwise number of bytes except padding
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
        for (i = 0; dstsize && srcsize && *src != '\n'; i++, src++, srcsize--)
        {
            if (*src == '\t')
            {
                int j = TAB_SKIP (ts, i + base);
                i += j - 1;
                while (j-- > 0)
                {
                    if (skip)
                    {
                        skip--;
                    }
                    else if (dstsize)
                    {
                        dstsize--;
                        *dst++ = ' ';
                    }
                }
            }
            else if (src[0] == '\r' && (srcsize == 1 || src[1] == '\n'))
            {
                if (!skip && show_cr)
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
            else
            {
                if (skip)
                {
                    skip--;
                }
                else
                {
                    dstsize--;
                    *dst++ = is_printable (*src) ? *src : '.';
                }
            }
        }
        sz = dst - tmp;
    }
    while (dstsize)
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
 * \param src buffer to read from
 * \param srcsize size of src buffer
 * \param dst buffer to read to
 * \param dstsize size of dst buffer, excluding trailing null
 * \param skip number of characters to skip
 * \param ts tab size
 * \param show_cr show trailing carriage return as ^M
 * \param hdiff horizontal diff structure
 * \param ord 0 if reading from first file, 1 if reading from 2nd file
 * \param att buffer of attributes
 *
 * \return negative on error, otherwise number of bytes except padding
 */
static int
cvt_mgeta (const char *src, size_t srcsize, char *dst, int dstsize, int skip, int ts, int show_cr,
           GArray * hdiff, int ord, char *att)
{
    int sz = 0;
    if (src != NULL)
    {
        int i, k;
        char *tmp = dst;
        const int base = 0;
        for (i = 0, k = 0; dstsize && srcsize && *src != '\n'; i++, k++, src++, srcsize--)
        {
            if (*src == '\t')
            {
                int j = TAB_SKIP (ts, i + base);
                i += j - 1;
                while (j-- > 0)
                {
                    if (skip)
                    {
                        skip--;
                    }
                    else if (dstsize)
                    {
                        dstsize--;
                        *att++ = is_inside (k, hdiff, ord);
                        *dst++ = ' ';
                    }
                }
            }
            else if (src[0] == '\r' && (srcsize == 1 || src[1] == '\n'))
            {
                if (!skip && show_cr)
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
            else
            {
                if (skip)
                {
                    skip--;
                }
                else
                {
                    dstsize--;
                    *att++ = is_inside (k, hdiff, ord);
                    *dst++ = is_printable (*src) ? *src : '.';
                }
            }
        }
        sz = dst - tmp;
    }
    while (dstsize)
    {
        dstsize--;
        *att++ = 0;
        *dst++ = ' ';
    }
    *dst = '\0';
    return sz;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read line from file, converting tabs to spaces and padding with spaces.
 *
 * \param f file stream to read from
 * \param off offset of line inside file
 * \param dst buffer to read to
 * \param dstsize size of dst buffer, excluding trailing null
 * \param skip number of characters to skip
 * \param ts tab size
 * \param show_cr show trailing carriage return as ^M
 *
 * \return negative on error, otherwise number of bytes except padding
 */
static int
cvt_fget (FBUF * f, off_t off, char *dst, size_t dstsize, int skip, int ts, int show_cr)
{
    int base = 0;
    int old_base = base;
    const int amount = dstsize;

    size_t useful, offset;

    size_t i;
    size_t sz;

    int lastch = '\0';

    const char *q = NULL;
    char tmp[BUFSIZ];           /* XXX capacity must be >= max{dstsize + 1, amount} */
    char cvt[BUFSIZ];           /* XXX capacity must be >= MAX_TAB_WIDTH * amount */

    if ((int) sizeof (tmp) < amount || (int) sizeof (tmp) <= dstsize
        || (int) sizeof (cvt) < 8 * amount)
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
        if (!(sz = f_gets (tmp, amount, f)))
        {
            break;
        }
        base = cvt_cpy (cvt, tmp, sz, old_base, ts);
        if (cvt[base - old_base - 1] == '\n')
        {
            q = &cvt[base - old_base - 1];
            base = old_base + q - cvt + 1;
            break;
        }
    }

    useful = base - skip;
    offset = skip - old_base;

    if (useful < 0)
    {
        memset (dst, ' ', dstsize);
        dst[dstsize] = '\0';
        return 0;
    }

    if (useful <= dstsize)
    {
        if (useful)
        {
            memmove (dst, cvt + offset, useful);
        }
        if (q == NULL && (sz = f_gets (tmp, dstsize - useful + 1, f)))
        {
            const char *ptr = tmp;
            useful += cvt_ncpy (dst + useful, dstsize - useful, &ptr, sz, base, ts) - base;
            if (ptr < tmp + sz)
            {
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
                    dst[i++] = '.';
                }
            }
            break;
        }
        else if (!is_printable (dst[i]))
        {
            dst[i] = '.';
        }
    }
    for (; i < dstsize; i++)
    {
        dst[i] = ' ';
    }
    dst[i] = '\0';
    return sz;
}

/* --------------------------------------------------------------------------------------------- */
/* diff printers et al ****************************************************** */

static void
cc_free_elt (void *elt)
{
    DIFFLN *p = elt;
    if (p->p)
    {
        g_free (p->p);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
printer (void *ctx, int ch, int line, off_t off, size_t sz, const char *str)
{
    GArray *a = ((PRINTER_CTX *) ctx)->a;
    DSRC dsrc = ((PRINTER_CTX *) ctx)->dsrc;
    if (ch)
    {
        DIFFLN p;
        p.p = NULL;
        p.ch = ch;
        p.line = line;
        p.u.off = off;
        if (dsrc == DATA_SRC_MEM && line)
        {
            if (sz && str[sz - 1] == '\n')
            {
                sz--;
            }
            if (sz)
            {
                p.p = g_malloc (sz);
                memcpy (p.p, str, sz);
            }
            p.u.len = sz;
        }
        g_array_append_val (a, p);
    }
    else if (dsrc == DATA_SRC_MEM)
    {
        DIFFLN *p;
        p = &g_array_index (a, DIFFLN, a->len - 1);
        if (sz && str[sz - 1] == '\n')
        {
            sz--;
        }
        if (sz)
        {
            size_t new_size = p->u.len + sz;
            char *q = g_realloc (p->p, new_size);
            memcpy (q + p->u.len, str, sz);
            p->p = q;
        }
        p->u.len += sz;
    }
    if (dsrc == DATA_SRC_TMP && (line || !ch))
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
    {
        strcat (extra, " -d");
    }
    if (dview->opt.quality == 1)
    {
        strcat (extra, " --speed-large-files");
    }
    if (dview->opt.strip_trailing_cr)
    {
        strcat (extra, " --strip-trailing-cr");
    }
    if (dview->opt.ignore_tab_expansion)
    {
        strcat (extra, " -E");
    }
    if (dview->opt.ignore_space_change)
    {
        strcat (extra, " -b");
    }
    if (dview->opt.ignore_all_space)
    {
        strcat (extra, " -w");
    }
    if (dview->opt.ignore_case)
    {
        strcat (extra, " -i");
    }

    if (dview->dsrc != DATA_SRC_MEM)
    {
        f_reset (f[0]);
        f_reset (f[1]);
    }

    ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    ndiff = dff_execute (dview->args, extra, dview->file[0], dview->file[1], ops);
    if (ndiff < 0)
    {
        g_array_free (ops, TRUE);
        return -1;
    }

    ctx.dsrc = dview->dsrc;

    rv = 0;
    ctx.a = dview->a[0];
    ctx.f = f[0];
    rv |= dff_reparse (0, dview->file[0], ops, printer, &ctx);

    ctx.a = dview->a[1];
    ctx.f = f[1];
    rv |= dff_reparse (1, dview->file[1], ops, printer, &ctx);

    g_array_free (ops, TRUE);

    if (rv != 0 || dview->a[0]->len != dview->a[1]->len)
        return -1;

    if (dview->dsrc == DATA_SRC_TMP)
    {
        f_trunc (f[0]);
        f_trunc (f[1]);
    }

    if (dview->dsrc == DATA_SRC_MEM && HDIFF_ENABLE)
    {
        dview->hdiff = g_ptr_array_new ();
        if (dview->hdiff != NULL)
        {
            size_t i;
            const DIFFLN *p;
            const DIFFLN *q;
            for (i = 0; i < dview->a[0]->len; i++)
            {
                GArray *h = NULL;
                p = &g_array_index (dview->a[0], DIFFLN, i);
                q = &g_array_index (dview->a[1], DIFFLN, i);
                if (p->line && q->line && p->ch == CHG_CH)
                {
                    h = g_array_new (FALSE, FALSE, sizeof (BRACKET));
                    if (h != NULL)
                    {
                        gboolean runresult =
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
        int len = dview->a[0]->len;
        for (i = 0; i < len; i++)
        {
            GArray *h = (GArray *) g_ptr_array_index (dview->hdiff, i);
            if (h != NULL)
            {
                g_array_free (h, TRUE);
            }
        }
        g_ptr_array_free (dview->hdiff, TRUE);
        dview->hdiff = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* stuff ******************************************************************** */

static int
get_digits (unsigned int n)
{
    int d = 1;
    while (n /= 10)
    {
        d++;
    }
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_line_numbers (const GArray * a, size_t pos, int *linenum, int *lineofs)
{
    const DIFFLN *p;

    *linenum = 0;
    *lineofs = 0;

    if (a->len)
    {
        if (pos >= a->len)
        {
            pos = a->len - 1;
        }

        p = &g_array_index (a, DIFFLN, pos);

        if (!p->line)
        {
            int n;
            for (n = pos; n > 0; n--)
            {
                p--;
                if (p->line)
                {
                    break;
                }
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
    get_line_numbers (a[0], a[0]->len - 1, &l1, &o1);
    get_line_numbers (a[1], a[1]->len - 1, &l2, &o2);
    if (l1 < l2)
    {
        l1 = l2;
    }
    return get_digits (l1);
}

/* --------------------------------------------------------------------------------------------- */

static int
find_prev_hunk (const GArray * a, int pos)
{
#if 1
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch != EQU_CH)
    {
        pos--;
    }
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch == EQU_CH)
    {
        pos--;
    }
#else
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos - 1))->ch == EQU_CH)
    {
        pos--;
    }
    while (pos > 0 && ((DIFFLN *) & g_array_index (a, DIFFLN, pos - 1))->ch != EQU_CH)
    {
        pos--;
    }
#endif

    return pos;
}

/* --------------------------------------------------------------------------------------------- */

static int
find_next_hunk (const GArray * a, size_t pos)
{
    while (pos < a->len && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch != EQU_CH)
    {
        pos++;
    }
    while (pos < a->len && ((DIFFLN *) & g_array_index (a, DIFFLN, pos))->ch == EQU_CH)
    {
        pos++;
    }

    return pos;
}

/* --------------------------------------------------------------------------------------------- */
/* view routines and callbacks ********************************************** */

static void
view_compute_split (WDiff * dview, int i)
{
    dview->bias += i;
    if (dview->bias < 2 - dview->half1)
    {
        dview->bias = 2 - dview->half1;
    }
    if (dview->bias > dview->half2 - 2)
    {
        dview->bias = dview->half2 - 2;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_compute_areas (WDiff * dview)
{
    dview->height = LINES - 2;
    dview->half1 = COLS / 2;
    dview->half2 = COLS - dview->half1;

    view_compute_split (dview, 0);
}

/* --------------------------------------------------------------------------------------------- */

static int
view_init (WDiff * dview, const char *args, const char *file1, const char *file2,
           const char *label1, const char *label2, DSRC dsrc)
{
    int ndiff;
    FBUF *f[2];

    f[0] = NULL;
    f[1] = NULL;

    if (dsrc == DATA_SRC_TMP)
    {
        f[0] = f_temp ();
        if (f[0] == NULL)
        {
            goto err_2;
        }
        f[1] = f_temp ();
        if (f[1] == NULL)
        {
            f_close (f[0]);
            goto err_2;
        }
    }
    if (dsrc == DATA_SRC_ORG)
    {
        f[0] = f_open (file1, O_RDONLY);
        if (f[0] == NULL)
        {
            goto err_2;
        }
        f[1] = f_open (file2, O_RDONLY);
        if (f[1] == NULL)
        {
            f_close (f[0]);
            goto err_2;
        }
    }

    dview->args = args;
    dview->file[0] = file1;
    dview->file[1] = file2;
    dview->label[0] = label1;
    dview->label[1] = label2;
    dview->f[0] = f[0];
    dview->f[1] = f[1];
    dview->hdiff = NULL;
    dview->dsrc = dsrc;

    dview->a[0] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));
    dview->a[1] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));

    ndiff = redo_diff (dview);
    if (ndiff < 0)
    {
        g_array_foreach (dview->a[0], DIFFLN, cc_free_elt);
        g_array_free (dview->a[0], TRUE);
        g_array_foreach (dview->a[1], DIFFLN, cc_free_elt);
        g_array_free (dview->a[1], TRUE);
        goto err_3;
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
    dview->ord = 0;
    dview->full = 0;
    dview->last_found = -1;

    dview->opt.quality = 0;
    dview->opt.strip_trailing_cr = 0;
    dview->opt.ignore_tab_expansion = 0;
    dview->opt.ignore_space_change = 0;
    dview->opt.ignore_all_space = 0;
    dview->opt.ignore_case = 0;

    view_compute_areas (dview);
    return 0;

  err_3:
    if (dsrc != DATA_SRC_MEM)
    {
        f_close (f[1]);
        f_close (f[0]);
    }
  err_2:
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
view_reinit (WDiff * dview)
{
    const char *quality_str[] = {
        N_("&Normal"),
        N_("&Fastest"),
        N_("&Minimal")
    };

    QuickWidget diffopt_widgets[] = {
        QUICK_BUTTON (6, 10, 13, OPTY, N_("&Cancel"), B_CANCEL, NULL),
        QUICK_BUTTON (2, 10, 13, OPTY, N_("&OK"), B_ENTER, NULL),
        QUICK_RADIO (3, OPTX, 15, OPTX,
                     3, (const char **) quality_str, (int *) &dview->opt.quality),
        QUICK_CHECKBOX (3, OPTX, 11, OPTY,
                        N_("strip trailing &CR"), &dview->opt.strip_trailing_cr),
        QUICK_CHECKBOX (3, OPTX, 10, OPTY,
                        N_("ignore all &Whitespace"), &dview->opt.ignore_all_space),
        QUICK_CHECKBOX (3, OPTX, 9, OPTY,
                        N_("ignore &Space change"), &dview->opt.ignore_space_change),
        QUICK_CHECKBOX (3, OPTX, 8, OPTY,
                        N_("ignore tab &Expansion"), &dview->opt.ignore_tab_expansion),
        QUICK_CHECKBOX (3, OPTX, 7, OPTY,
                        N_("&Ignore case"), &dview->opt.ignore_case),
        QUICK_END
    };

    QuickDialog diffopt = {
        OPTX, OPTY, -1, -1,
        N_("Diff Options"), "[Diff Options]",
        diffopt_widgets, 0
    };

    int ndiff = dview->ndiff;
    if (quick_dialog (&diffopt) != B_CANCEL)
    {
        destroy_hdiff (dview);

        g_array_foreach (dview->a[0], DIFFLN, cc_free_elt);
        g_array_free (dview->a[0], TRUE);
        g_array_foreach (dview->a[1], DIFFLN, cc_free_elt);
        g_array_free (dview->a[1], TRUE);

        dview->a[0] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));
        dview->a[1] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));

        ndiff = redo_diff (dview);
        if (ndiff >= 0)
        {
            dview->ndiff = ndiff;
        }
    }
    return ndiff;
}

/* --------------------------------------------------------------------------------------------- */

static void
view_fini (WDiff * dview)
{
    if (dview->dsrc != DATA_SRC_MEM)
    {
        f_close (dview->f[1]);
        f_close (dview->f[0]);
    }

    destroy_hdiff (dview);
    g_array_foreach (dview->a[0], DIFFLN, cc_free_elt);
    g_array_free (dview->a[0], TRUE);
    g_array_foreach (dview->a[1], DIFFLN, cc_free_elt);
    g_array_free (dview->a[1], TRUE);

    dview->a[1] = NULL;
    dview->a[0] = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
view_display_file (const WDiff * dview, int ord, int r, int c, int height, int width)
{
    size_t i;
    int j, k;
    char buf[BUFSIZ];
    FBUF *f = dview->f[ord];
    int skip = dview->skip_cols;
    int display_symbols = dview->display_symbols;
    int display_numbers = dview->display_numbers;
    int show_cr = dview->show_cr;
    int tab_size = dview->tab_size;
    const DIFFLN *p;

    int nwidth = display_numbers;
    int xwidth = display_symbols + display_numbers;

    if (xwidth)
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
        {
            width = 0;
        }
    }

    if ((int) sizeof (buf) <= width || (int) sizeof (buf) <= nwidth)
    {
        /* abnormal, but avoid buffer overflow */
        return -1;
    }

    for (i = dview->skip_rows, j = 0; i < dview->a[ord]->len && j < height; j++, i++)
    {
        int ch;
        p = (DIFFLN *) & g_array_index (dview->a[ord], DIFFLN, i);
        ch = p->ch;
        tty_setcolor (NORMAL_COLOR);
        if (display_symbols)
        {
            tty_gotoyx (r + j, c - 2);
            tty_print_char (ch);
        }
        if (p->line)
        {
            if (display_numbers)
            {
                tty_gotoyx (r + j, c - xwidth);
                g_snprintf (buf, display_numbers + 1, "%*d", nwidth, p->line);
                tty_print_string (str_fit_to_term (buf, nwidth, J_LEFT_FIT));
            }
            if (ch == ADD_CH)
            {
                tty_setcolor (DFF_ADD_COLOR);
            }
            if (ch == CHG_CH)
            {
                tty_setcolor (DFF_CHG_COLOR);
            }
            if (f == NULL)
            {
                if (i == (size_t) dview->last_found)
                {
                    tty_setcolor (MARKED_SELECTED_COLOR);
                }
                else
                {
                    if (dview->hdiff != NULL && g_ptr_array_index (dview->hdiff, i) != NULL)
                    {
                        char att[BUFSIZ];
                        cvt_mgeta (p->p, p->u.len, buf, width, skip, tab_size, show_cr,
                                   g_ptr_array_index (dview->hdiff, i), ord, att);
                        tty_gotoyx (r + j, c);
                        for (k = 0; k < width; k++)
                        {
                            tty_setcolor (att[k] ? DFF_CHH_COLOR : DFF_CHG_COLOR);
                            tty_print_char (buf[k]);
                        }
                        continue;
                    }
                    else if (ch == CHG_CH)
                    {
                        tty_setcolor (DFF_CHH_COLOR);
                    }
                }
                cvt_mget (p->p, p->u.len, buf, width, skip, tab_size, show_cr);
            }
            else
            {
                cvt_fget (f, p->u.off, buf, width, skip, tab_size, show_cr);
            }
        }
        else
        {
            if (display_numbers)
            {
                tty_gotoyx (r + j, c - xwidth);
                memset (buf, ' ', display_numbers);
                buf[display_numbers] = '\0';
                /* tty_print_nstring (buf, display_numbers); */
                tty_print_string (buf);
            }
            if (ch == DEL_CH)
            {
                tty_setcolor (DFF_DEL_COLOR);
            }
            if (ch == CHG_CH)
            {
                tty_setcolor (DFF_CHD_COLOR);
            }
            memset (buf, ' ', width);
            buf[width] = '\0';
        }
        tty_gotoyx (r + j, c);
        /* tty_print_nstring (buf, width); */
        tty_print_string (buf);
    }
    tty_setcolor (NORMAL_COLOR);
    k = width;
    if (width < xwidth - 1)
    {
        k = xwidth - 1;
    }
    memset (buf, ' ', k);
    buf[k] = '\0';
    for (; j < height; j++)
    {
        if (xwidth)
        {
            tty_gotoyx (r + j, c - xwidth);
            /* tty_print_nstring (buf, xwidth - 1); */
            tty_print_string (buf);
        }
        tty_gotoyx (r + j, c);
        /* tty_print_nstring (buf, width); */
        tty_print_string (buf);
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
view_status (const WDiff * dview, int ord, int width, int c)
{
    int skip_rows = dview->skip_rows;
    int skip_cols = dview->skip_cols;

    char buf[BUFSIZ];
    int filename_width;
    int linenum, lineofs;

    tty_setcolor (SELECTED_COLOR);

    tty_gotoyx (0, c);
    get_line_numbers (dview->a[ord], skip_rows, &linenum, &lineofs);

    filename_width = width - 22;
    if (filename_width < 8)
    {
        filename_width = 8;
    }
    if (filename_width >= (int) sizeof (buf))
    {
        /* abnormal, but avoid buffer overflow */
        filename_width = sizeof (buf) - 1;
    }
    trim (strip_home_and_password (dview->label[ord]), buf, filename_width);
    if (ord == 0)
    {
        tty_printf ("%-*s %6d+%-4d Col %-4d ", filename_width, buf, linenum, lineofs, skip_cols);
    }
    else
    {
        tty_printf ("%-*s %6d+%-4d Dif %-4d ", filename_width, buf, linenum, lineofs, dview->ndiff);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_update (WDiff * dview)
{
    int height = dview->height;
    int width1;
    int width2;

    int last = dview->a[0]->len - 1;

    if (dview->skip_rows > last)
    {
        dview->skip_rows = last;
    }
    if (dview->skip_rows < 0)
    {
        dview->skip_rows = 0;
    }
    if (dview->skip_cols < 0)
    {
        dview->skip_cols = 0;
    }

    if (height < 2)
    {
        return;
    }

    width1 = dview->half1 + dview->bias;
    width2 = dview->half2 - dview->bias;
    if (dview->full)
    {
        width1 = COLS;
        width2 = 0;
    }

    if (dview->new_frame)
    {
        int xwidth = dview->display_symbols + dview->display_numbers;

        tty_setcolor (NORMAL_COLOR);
        if (width1 > 1)
        {
            tty_draw_box (1, 0, height, width1, FALSE);
        }
        if (width2 > 1)
        {
            tty_draw_box (1, width1, height, width2, FALSE);
        }

        if (xwidth)
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
        view_status (dview, dview->ord, width1, 0);
        view_display_file (dview, dview->ord, 2, 1, height - 2, width1 - 2);
    }
    if (width2 > 2)
    {
        view_status (dview, dview->ord ^ 1, width2, width1);
        view_display_file (dview, dview->ord ^ 1, 2, width1 + 1, height - 2, width2 - 2);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_redo (WDiff * dview)
{
    if (view_reinit (dview) < 0)
    {
        dview->view_quit = 1;
    }
    else if (dview->display_numbers)
    {
        int old = dview->display_numbers;
        dview->display_numbers = calc_nwidth ((const GArray **) dview->a);
        dview->new_frame = (old != dview->display_numbers);
    }
}

/* --------------------------------------------------------------------------------------------- */

static const unsigned char *
memmem_dummy (const unsigned char *haystack, size_t i, size_t hlen, const unsigned char *needle,
              size_t nlen, int whole)
{
    for (; i + nlen <= hlen; i++)
    {
        if (haystack[i] == needle[0])
        {
            size_t j;
            for (j = 1; j < nlen; j++)
            {
                if (haystack[i + j] != needle[j])
                {
                    break;
                }
            }
            if (j == nlen && IS_WHOLE_OR_DONT_CARE ())
            {
                return haystack + i;
            }
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const unsigned char *
memmem_dummy_nocase (const unsigned char *haystack, size_t i, size_t hlen,
                     const unsigned char *needle, size_t nlen, int whole)
{
    for (; i + nlen <= hlen; i++)
    {
        if (toupper (haystack[i]) == toupper (needle[0]))
        {
            size_t j;
            for (j = 1; j < nlen; j++)
            {
                if (toupper (haystack[i + j]) != toupper (needle[j]))
                {
                    break;
                }
            }
            if (j == nlen && IS_WHOLE_OR_DONT_CARE ())
            {
                return haystack + i;
            }
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const unsigned char *
search_string (const DIFFLN * p, size_t xpos, const void *needle, size_t nlen, int whole, int ccase)
{
    const unsigned char *haystack = p->p;
    size_t hlen = p->u.len;

    if (xpos > hlen || nlen <= 0 || haystack == NULL || needle == NULL)
    {
        return NULL;
    }

    /* XXX I should use Boyer-Moore */
    if (ccase)
    {
        return memmem_dummy (haystack, xpos, hlen, needle, nlen, whole);
    }
    else
    {
        return memmem_dummy_nocase (haystack, xpos, hlen, needle, nlen, whole);
    }
}


/* --------------------------------------------------------------------------------------------- */

static int
view_search_string (WDiff * dview, const char *needle, int ccase, int back, int whole)
{
    size_t nlen = strlen (needle);
    size_t xpos = 0;

    int ord = dview->ord;
    const DIFFLN *p;

    size_t i = (size_t) dview->last_found;

    if (back)
    {
        if (i == (size_t) - 1)
        {
            i = dview->skip_rows;
        }
        for (--i; i >= 0; i--)
        {
            const unsigned char *q;

            p = (DIFFLN *) & g_array_index (dview->a[ord], DIFFLN, i);
            q = search_string (p, xpos, needle, nlen, whole, ccase);
            if (q != NULL)
            {
                return i;
            }
        }
    }
    else
    {
        if (i == (size_t) - 1)
        {
            i = dview->skip_rows - 1;
        }
        for (++i; i < dview->a[ord]->len; i++)
        {
            const unsigned char *q;

            p = (DIFFLN *) & g_array_index (dview->a[ord], DIFFLN, i);
            q = search_string (p, xpos, needle, nlen, whole, ccase);
            if (q != NULL)
            {
                return i;
            }
        }
    }

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static void
view_search (WDiff * dview, int again)
{
    /* XXX some statics here, to be remembered between runs */
    static char *searchopt_text = NULL;
    static int searchopt_type;
    static int searchopt_case;
    static int searchopt_backwards;
    static int searchopt_whole;

    static int compiled = 0;

    if (again < 0)
    {
        g_free (searchopt_text);
        searchopt_text = NULL;
        if (compiled)
        {
            compiled = 0;
            /*XXX free search exp */
        }
        return;
    }

    if (dview->dsrc != DATA_SRC_MEM)
    {
        error_dialog (_("Search"), _(" Search is disabled "));
        return;
    }

    if (!again || searchopt_text == NULL)
    {
        char *tsearchopt_text;
        int tsearchopt_type = searchopt_type;
        int tsearchopt_case = searchopt_case;
        int tsearchopt_backwards = searchopt_backwards;
        int tsearchopt_whole = searchopt_whole;

        const char *search_str[] = {
            N_("&Normal")
        };

        QuickWidget search_widgets[] = {
            /* 0 */
            QUICK_BUTTON (6, 10, 11, SEARCH_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
            /* 1 */
            QUICK_BUTTON (4, 10, 11, SEARCH_DLG_HEIGHT, N_("&Find all"), B_USER, NULL),
            /* 2 */
            QUICK_BUTTON (2, 10, 11, SEARCH_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
            /* 3 */
            QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 8, SEARCH_DLG_HEIGHT, N_("&Whole words"),
                            &tsearchopt_whole),
            /* 4 */
            QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("&Backwards"),
                            &tsearchopt_backwards),
            /* 5 */
            QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("case &Sensitive"),
                            &tsearchopt_case),
            /* 6 */
            QUICK_RADIO (3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
                         1, (const char **) search_str, (int *) &tsearchopt_type),
            /* 7 */
            QUICK_INPUT (3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT,
                         tsearchopt_text, SEARCH_DLG_WIDTH - 6, 0, MC_HISTORY_SHARED_SEARCH,
                         &tsearchopt_text),
            /* 8 */
            QUICK_LABEL (2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_(" Enter search string:")),
            QUICK_END
        };

        QuickDialog search_input = {
            SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0,
            N_("Search"), "[Input Line Keys]",
            search_widgets, 0
        };

        if (quick_dialog (&search_input) == B_CANCEL)
        {
            return;
        }
        if (tsearchopt_text == NULL || !*tsearchopt_text)
        {
            g_free (tsearchopt_text);
            return;
        }
        g_free (searchopt_text);

        searchopt_text = tsearchopt_text;
        searchopt_type = tsearchopt_type;
        searchopt_case = tsearchopt_case;
        searchopt_backwards = tsearchopt_backwards;
        searchopt_whole = tsearchopt_whole;
    }

    if (compiled)
    {
        compiled = 0;
        /*XXX free search exp */
    }
    if (0 /*XXX new search exp */ )
    {
        error_dialog (_("Error"), _(" Cannot search "));
        return;
    }
    compiled = 1;
    if (searchopt_type == 0)
    {
        dview->last_found =
            view_search_string (dview, searchopt_text, searchopt_case, searchopt_backwards,
                                searchopt_whole);
    }

    if (dview->last_found == -1)
    {
        error_dialog (_("Search"), _(" Search string not found "));
    }
    else
    {
        dview->skip_rows = dview->last_found;
        view_update (dview);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_edit (WDiff * dview, int ord)
{
    int linenum, lineofs;

    if (dview->dsrc == DATA_SRC_TMP)
    {
        error_dialog (_("Edit"), _(" Edit is disabled "));
        return;
    }

    get_line_numbers (dview->a[ord], dview->skip_rows, &linenum, &lineofs);
    do_edit_at_line (dview->file[ord], linenum);
    view_redo (dview);
    view_update (dview);
}

/* --------------------------------------------------------------------------------------------- */

static void
view_goto_cmd (WDiff * dview, int ord)
{
    static const char *title[2] = { " Goto line (left) ", " Goto line (right) " };
    static char prev[256];
    /* XXX some statics here, to be remembered between runs */

    int newline;
    char *input;

    input = input_dialog (_(title[ord]), _(" Enter line: "), MC_HISTORY_YDIFF_GOTO_LINE, prev);
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
                    {
                        break;
                    }
                }
            }
            dview->skip_rows = i;
            snprintf (prev, sizeof (prev), "%d", newline);
        }
        g_free (input);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_labels (WDiff * dview)
{
    Dlg_head *h = dview->widget.parent;
    WButtonBar *b = find_buttonbar (h);

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), diff_map, (Widget *) dview);
    buttonbar_set_label (b, 4, Q_ ("ButtonBar|Edit"), diff_map, (Widget *) dview);
    buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), diff_map, (Widget *) dview);
    buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), diff_map, (Widget *) dview);
}


/* --------------------------------------------------------------------------------------------- */

static int
view_event (Gpm_Event * event, void *x)
{
    WDiff *dview = (WDiff *) x;
    int result = MOU_NORMAL;

    /* We are not interested in the release events */
    if (!(event->type & (GPM_DOWN | GPM_DRAG)))
    {
        return result;
    }

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN))
    {
        dview->skip_rows -= 2;
        view_update (dview);
        return result;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN))
    {
        dview->skip_rows += 2;
        view_update (dview);
        return result;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
view_execute_cmd (WDiff * dview, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    switch (command)
    {
    case CK_DiffDisplaySymbols:
        dview->display_symbols ^= 1;
        dview->new_frame = 1;
        break;
    case CK_DiffDisplayNumbers:
        dview->display_numbers ^= calc_nwidth ((const GArray ** const) dview->a);
        dview->new_frame = 1;
        break;
    case CK_DiffFull:
        dview->full ^= 1;
        dview->new_frame = 1;
        break;
    case CK_DiffEqual:
        if (!dview->full)
        {
            dview->bias = 0;
            dview->new_frame = 1;
        }
        break;
    case CK_DiffSplitMore:
        if (!dview->full)
        {
            view_compute_split (dview, 1);
            dview->new_frame = 1;
        }
        break;

    case CK_DiffSplitLess:
        if (!dview->full)
        {
            view_compute_split (dview, -1);
            dview->new_frame = 1;
        }
        break;
    case CK_DiffShowCR:
        dview->show_cr ^= 1;
        break;
    case CK_DiffSetTab2:
        dview->tab_size = 2;
        break;
    case CK_DiffSetTab3:
        dview->tab_size = 3;
        break;
    case CK_DiffSetTab4:
        dview->tab_size = 4;
        break;
    case CK_DiffSetTab8:
        dview->tab_size = 8;
        break;
    case CK_DiffSwapPanel:
        dview->ord ^= 1;
        break;
    case CK_DiffRedo:
        view_redo (dview);
        break;
    case CK_DiffNextHunk:
        dview->skip_rows = find_next_hunk (dview->a[0], dview->skip_rows);
        break;
    case CK_DiffPrevHunk:
        dview->skip_rows = find_prev_hunk (dview->a[0], dview->skip_rows);
        break;
    case CK_DiffGoto:
        view_goto_cmd (dview, TRUE);
        break;
        /* what this?
           case KEY_BACKSPACE:
           dview->last_found = -1;
           break;
         */
    case CK_DiffEditCurrent:
        view_edit (dview, dview->ord);
        break;
    case CK_DiffEditOther:
        view_edit (dview, dview->ord ^ 1);
        break;
    case CK_DiffSearch:
        view_search (dview, 1);
        break;
    case CK_DiffBOF:
        dview->skip_rows = 0;
        break;
    case CK_DiffEOF:
        dview->skip_rows = dview->a[0]->len - 1;
        break;
    case CK_DiffUp:
        dview->skip_rows--;
        break;
    case CK_DiffDown:
        dview->skip_rows++;
        break;
    case CK_DiffPageDown:
        dview->skip_rows += dview->height - 2;
        break;
    case CK_DiffPageUp:
        dview->skip_rows -= dview->height - 2;
        break;
    case CK_DiffLeft:
        dview->skip_cols--;
        break;
    case CK_DiffRight:
        dview->skip_cols++;
        break;
    case CK_DiffQuickLeft:
        dview->skip_cols -= 8;
        break;
    case CK_DiffQuickRight:
        dview->skip_cols += 8;
        break;
    case CK_DiffHome:
        dview->skip_cols = 0;
        break;
    case CK_ShowCommandLine:
        view_other_cmd ();
        break;
    case CK_DiffQuit:
        dview->view_quit = 1;
        break;
    default:
        res = MSG_NOT_HANDLED;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
view_handle_key (WDiff * dview, int key)
{
    unsigned long command;

    key = convert_from_input_c (key);

    command = lookup_keymap_command (diff_map, key);
    if ((command != CK_Ignore_Key) && (view_execute_cmd (dview, command) == MSG_HANDLED))
        return MSG_HANDLED;

    /* Key not used */
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
view_callback (Widget * w, widget_msg_t msg, int parm)
{
    WDiff *dview = (WDiff *) w;
    Dlg_head *h = dview->widget.parent;
    cb_ret_t i;

    switch (msg)
    {
    case WIDGET_INIT:
        view_labels (dview);
        return MSG_HANDLED;

    case WIDGET_DRAW:
        dview->new_frame = 1;
        view_update (dview);
        return MSG_HANDLED;

    case WIDGET_CURSOR:
        return MSG_HANDLED;

    case WIDGET_KEY:
        i = view_handle_key (dview, parm);
        if (dview->view_quit)
            dlg_stop (h);
        else
        {
            view_update (dview);
        }
        return i;

    case WIDGET_IDLE:
        return MSG_HANDLED;

    case WIDGET_FOCUS:
        view_labels (dview);
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
view_adjust_size (Dlg_head * h)
{
    WDiff *dview;
    WButtonBar *bar;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    dview = (WDiff *) find_widget_type (h, view_callback);
    bar = find_buttonbar (h);
    widget_set_size (&dview->widget, 0, 0, LINES, COLS);
    widget_set_size ((Widget *) bar, LINES - 1, 0, 1, COLS);

    view_compute_areas (dview);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
view_dialog_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_RESIZE:
        view_adjust_size (h);
        return MSG_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
diff_view (const char *file1, const char *file2, const char *label1, const char *label2)
{
    int error;
    WDiff *dview;
    WButtonBar *bar;
    Dlg_head *view_dlg;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg =
        create_dlg (0, 0, LINES, COLS, NULL, view_dialog_callback,
                    "[Diff Viewer]", NULL, DLG_WANT_TAB);

    dview = g_new0 (WDiff, 1);

    init_widget (&dview->widget, 0, 0, LINES, COLS,
                 (callback_fn) view_callback, (mouse_h) view_event);

    widget_want_cursor (dview->widget, 0);

    bar = buttonbar_new (1);

    add_widget (view_dlg, bar);
    add_widget (view_dlg, dview);

    error = view_init (dview, "-a", file1, file2, label1, label2, DATA_SRC_MEM);        /* XXX binary diff? */

    /* Please note that if you add another widget,
     * you have to modify view_adjust_size to
     * be aware of it
     */
    if (!error)
    {
        run_dlg (view_dlg);
        view_search (dview, -1);
        view_fini (dview);
    }
    destroy_dlg (view_dlg);

    return error;
}

/* --------------------------------------------------------------------------------------------- */

#define GET_FILE_AND_STAMP(n)					\
    do {							\
	use_copy##n = 0;					\
	real_file##n = file##n;					\
	if (!vfs_file_is_local(file##n)) {			\
	    real_file##n = mc_getlocalcopy(file##n);		\
	    if (real_file##n != NULL) {				\
		use_copy##n = 1;				\
		if (mc_stat(real_file##n, &st##n) != 0) {	\
		    use_copy##n = -1;				\
		}						\
	    }							\
	}							\
    } while (0)
#define UNGET_FILE(n)						\
    do {							\
	if (use_copy##n) {					\
	    int changed = 0;					\
	    if (use_copy##n > 0) {				\
		time_t mtime = st##n.st_mtime;			\
		if (mc_stat(real_file##n, &st##n) == 0) {	\
		    changed = (mtime != st##n.st_mtime);	\
		}						\
	    }							\
	    mc_ungetlocalcopy(file##n, real_file##n, changed);	\
	    g_free(real_file##n);				\
	}							\
    } while (0)

void
view_diff_cmd (WDiff *dview)
{
    int rv = 0;
    char *file0 = NULL;
    char *file1 = NULL;
    int is_dir0 = 0;
    int is_dir1 = 0;

    if (dview == NULL)
    {
        const WPanel *panel0 = current_panel;
        const WPanel *panel1 = other_panel;
        if (get_current_index ())
        {
            panel0 = other_panel;
            panel1 = current_panel;
        }
        file0 = concat_dir_and_file (panel0->cwd, selection (panel0)->fname);
        file1 = concat_dir_and_file (panel1->cwd, selection (panel1)->fname);
        is_dir0 = S_ISDIR (selection (panel0)->st.st_mode);
        is_dir1 = S_ISDIR (selection (panel1)->st.st_mode);
    }

    if (rv == 0) {
        rv = -1;
        if (file0 != NULL && !is_dir0 && file1 != NULL && !is_dir1)
        {
            int use_copy0;
            int use_copy1;
            struct stat st0;
            struct stat st1;
            char *real_file0;
            char *real_file1;
            GET_FILE_AND_STAMP (0);
            GET_FILE_AND_STAMP (1);
            if (real_file0 != NULL && real_file1 != NULL)
            {
                rv = diff_view (real_file0, real_file1, file0, file1);
            }
            UNGET_FILE (1);
            UNGET_FILE (0);
        }
    }

    g_free (file1);
    g_free (file0);

    if (rv != 0) {
        message (1, MSG_ERROR, _("Need two files to compare"));
    }
}
