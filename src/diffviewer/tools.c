/*
   Search functions for diffviewer.

   Copyright (C) 2010-2015
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010, 2015.
   Andrew Borodin <aborodin@vmail.ru>, 2012, 2015

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

#include <stdio.h>
#include <errno.h>

#include "lib/global.h"
#include "lib/event.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/skin.h"           /* EDITOR_NORMAL_COLOR */
#include "lib/strutil.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/util.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define g_array_foreach(a, TP, cbf) \
do { \
    size_t g_array_foreach_i;\
    \
    for (g_array_foreach_i = 0; g_array_foreach_i < a->len; g_array_foreach_i++) \
    { \
        TP *g_array_foreach_var; \
        \
        g_array_foreach_var = &g_array_index (a, TP, g_array_foreach_i); \
        (*cbf) (g_array_foreach_var); \
    } \
} while (0)

#define HDIFF_ENABLE 1
#define HDIFF_MINCTX 5
#define HDIFF_DEPTH 10

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
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

static int
get_digits (unsigned int n)
{
    int d = 1;

    while (n /= 10)
        d++;
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static void
cc_free_elt (void *elt)
{
    DIFFLN *p = elt;

    if (p != NULL)
        g_free (p->p);
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
    mc_diffviewer_get_line_numbers (dview->a[ord], dview->skip_rows, &linenum, &lineofs);

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
#ifdef HAVE_CHARSET
/**
 * Get utf multibyte char from string
 *
 * @param char * str, int * char_length, gboolean * result
 * @return int as utf character or 0 and result == FALSE if fail
 */

static int
dview_get_utf (char *str, int *char_length, gboolean * result)
{
    int res = -1;
    gunichar ch;
    int ch_len = 0;

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
        gchar *next_ch;
        ch = res;

        /* Calculate UTF-8 char length */
        next_ch = g_utf8_next_char (str);
        ch_len = next_ch - str;
    }
    *char_length = ch_len;

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
#endif /*HAVE_CHARSET */

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
#ifdef HAVE_CHARSET
                gboolean res;
                int ch_len = 1;

                (void) dview_get_utf ((char *) src, &ch_len, &res);

                if (ch_len > 1)
                    skip += ch_len - 1;
#endif
                skip--;
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
#ifdef HAVE_CHARSET
                gboolean res;
                int ch_len = 1;

                (void) dview_get_utf ((char *) src, &ch_len, &res);
                if (ch_len > 1)
                    skip += ch_len - 1;
#endif
                skip--;
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

    mc_diffviewer_file_seek (f, off, SEEK_SET);

    while (skip > base)
    {
        old_base = base;
        sz = mc_diffviewer_file_gets (tmp, amount, f);
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
            sz = mc_diffviewer_file_gets (tmp, dstsize - useful + 1, f);
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

#ifdef HAVE_CHARSET
                    if (dview->utf8)
                        k = dview_str_utf8_offset_to_pos (p->p, width);
                    else
#endif /* HAVE_CHARSET */
                        k = width;

                    cvt_mgeta (p->p, p->u.len, buf, k, skip, tab_size, show_cr,
                               g_ptr_array_index (dview->hdiff, i), ord, att);
                    tty_gotoyx (r + j, c);
                    col = 0;

                    for (cnt = 0; cnt < strlen (buf) && col < width; cnt++)
                    {
                        gboolean ch_res;

#ifdef HAVE_CHARSET
                        if (dview->utf8)
                        {
                            int ch_len;

                            next_ch = dview_get_utf (buf + cnt, &ch_len, &ch_res);
                            if (ch_len > 1)
                                cnt += ch_len - 1;

                            if (!g_unichar_isprint (next_ch))
                                next_ch = '.';
                        }
                        else
#endif /* HAVE_CHARSET */
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
#endif /* HAVE_CHARSET */
                            tty_print_anychar (next_ch);
                            col++;
                        }
                    }
                    continue;
                }

                if (ch == CHG_CH)
                    tty_setcolor (DFF_CHH_COLOR);

#ifdef HAVE_CHARSET
                if (dview->utf8)
                    k = dview_str_utf8_offset_to_pos (p->p, width);
                else
#endif /* HAVE_CHARSET */
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
            gboolean ch_res;

#ifdef HAVE_CHARSET
            if (dview->utf8)
            {
                int ch_len;

                next_ch = dview_get_utf (buf + cnt, &ch_len, &ch_res);
                if (ch_len > 1)
                    cnt += ch_len - 1;
                if (!g_unichar_isprint (next_ch))
                    next_ch = '.';
            }
            else
#endif /* HAVE_CHARSET */
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

    f = mc_diffviewer_file_open (filename, O_RDONLY);
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

        while (line < n && (sz = mc_diffviewer_file_gets (buf, sizeof (buf), f)) != 0)
        {
            line++;
            printer (ctx, EQU_CH, line, off, sz, buf);
            off += sz;
            while (buf[sz - 1] != '\n')
            {
                sz = mc_diffviewer_file_gets (buf, sizeof (buf), f);
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
            while (n != 0 && (sz = mc_diffviewer_file_gets (buf, sizeof (buf), f)) != 0)
            {
                line++;
                printer (ctx, ADD_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    sz = mc_diffviewer_file_gets (buf, sizeof (buf), f);
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
            while (n != 0 && (sz = mc_diffviewer_file_gets (buf, sizeof (buf), f)) != 0)
            {
                line++;
                printer (ctx, CHG_CH, line, off, sz, buf);
                off += sz;
                while (buf[sz - 1] != '\n')
                {
                    sz = mc_diffviewer_file_gets (buf, sizeof (buf), f);
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

    while ((sz = mc_diffviewer_file_gets (buf, sizeof (buf), f)) != 0)
    {
        line++;
        printer (ctx, EQU_CH, line, off, sz, buf);
        off += sz;
        while (buf[sz - 1] != '\n')
        {
            sz = mc_diffviewer_file_gets (buf, sizeof (buf), f);
            if (sz == 0)
            {
                printer (ctx, 0, 0, 0, 1, "\n");
                break;
            }
            printer (ctx, 0, 0, 0, sz, buf);
            off += sz;
        }
    }

    mc_diffviewer_file_close (f);
    return 0;

  err:
    mc_diffviewer_file_close (f);
    return -1;
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
        mc_diffviewer_file_write (f, str, sz);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
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

    g_free (Lcurr);
    g_free (Lprev);
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
                diff_place_t source_diff, diff_place_t dest_diff)
{
    int line;
    char buf[BUF_10K];
    FILE *f0;
    FILE *f1;

    f0 = fopen (dview->file[source_diff], "r");
    f1 = fopen (dview->file[dest_diff], "r");

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
 * Remove hunk from file.
 *
 * @param dview           WDiff widget
 * @param merge_file      file stream for writing data
 * @param from1           first line of hunk
 * @param to1             last line of hunk
 * @param merge_direction in what direction files should be merged
 */

static void
dview_remove_hunk (WDiff * dview, FILE * merge_file, int from1, int to1, diff_place_t diff_index)
{
    int line;
    char buf[BUF_10K];
    FILE *f0;

    f0 = fopen (dview->file[diff_index], "r");

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
                    diff_place_t source_diff, diff_place_t dest_diff)
{
    int line1 = 0, line2 = 0;
    char buf[BUF_10K];
    FILE *f0;
    FILE *f1;

    f0 = fopen (dview->file[source_diff], "r");
    f1 = fopen (dview->file[dest_diff], "r");

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
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_reread (WDiff * dview)
{
    int ndiff;

    mc_diffviewer_destroy_hdiff (dview);
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

    ndiff = mc_diffviewer_redo_diff (dview);
    if (ndiff >= 0)
        dview->ndiff = ndiff;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_update (WDiff * dview)
{
    int height = dview->height;
    int width1;
    int width2;
    int last;

    if (dview->view_quit)
        return;

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
                tty_print_alt_char (ACS_TTEE, FALSE);
                tty_gotoyx (height, xwidth);
                tty_print_alt_char (ACS_BTEE, FALSE);
                tty_draw_vline (2, xwidth, ACS_VLINE, height - 2);
            }
            if (xwidth < width2 - 1)
            {
                tty_gotoyx (1, width1 + xwidth);
                tty_print_alt_char (ACS_TTEE, FALSE);
                tty_gotoyx (height, width1 + xwidth);
                tty_print_alt_char (ACS_BTEE, FALSE);
                tty_draw_vline (2, width1 + xwidth, ACS_VLINE, height - 2);
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

int
mc_diffviewer_calc_nwidth (const GArray ** const a)
{
    int l1, o1;
    int l2, o2;

    mc_diffviewer_get_line_numbers (a[DIFF_LEFT], a[DIFF_LEFT]->len - 1, &l1, &o1);
    mc_diffviewer_get_line_numbers (a[DIFF_RIGHT], a[DIFF_RIGHT]->len - 1, &l2, &o2);
    if (l1 < l2)
        l1 = l2;
    return get_digits (l1);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_compute_split (WDiff * dview, int i)
{
    dview->bias += i;
    if (dview->bias < 2 - dview->half1)
        dview->bias = 2 - dview->half1;
    if (dview->bias > dview->half2 - 2)
        dview->bias = dview->half2 - 2;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_diffviewer_get_line_numbers (const GArray * a, size_t pos, int *linenum, int *lineofs)
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

void
mc_diffviewer_destroy_hdiff (WDiff * dview)
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
    MC_PTR_FREE (dview->search.last_string);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_deinit (WDiff * dview)
{
    if (dview->dsrc != DATA_SRC_MEM)
    {
        mc_diffviewer_file_close (dview->f[DIFF_RIGHT]);
        mc_diffviewer_file_close (dview->f[DIFF_LEFT]);
    }
#if HAVE_CHARSET
    if (dview->converter != str_cnv_from_term)
        str_close_conv (dview->converter);
#endif /* HAVE_CHARSET */

    mc_diffviewer_destroy_hdiff (dview);
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
/* event callback */

gboolean
mc_diffviewer_cmd_merge (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    diff_place_t source_diff = (diff_place_t) event_info->init_data;
    diff_place_t dest_diff;
    int from1, to1, from2, to2;
    int *source_from, *source_to, *dest_from, *dest_to;
    int hunk;

    switch (source_diff)
    {
    case DIFF_OTHER:
        source_diff = dview->ord ^ 1;
        break;
    case DIFF_CURRENT:
        source_diff = dview->ord;
        break;
    default:
        break;
    }

    switch (source_diff)
    {
    case DIFF_LEFT:
        dest_diff = DIFF_RIGHT;
        source_from = &from1;
        source_to = &to1;
        dest_from = &from2;
        dest_to = &to2;
        break;
    case DIFF_RIGHT:
        dest_diff = DIFF_LEFT;
        source_from = &from2;
        source_to = &to2;
        dest_from = &from1;
        dest_to = &to1;
        break;
    default:
        return FALSE;
    }
    hunk = get_current_hunk (dview, source_from, source_to, dest_from, dest_to);

    if (hunk > 0)
    {
        int merge_file_fd;
        FILE *merge_file;
        vfs_path_t *merge_file_name_vpath = NULL;

        if (!dview->merged[source_diff])
        {
            dview->merged[source_diff] =
                mc_util_make_backup_if_possible (dview->file[source_diff], "~~~");
            if (!dview->merged[source_diff])
            {
                message (D_ERROR, MSG_ERROR,
                         _("Cannot create backup file\n%s%s\n%s"),
                         dview->file[source_diff], "~~~", unix_error_string (errno));
                return TRUE;
            }
        }

        merge_file_fd = mc_mkstemps (&merge_file_name_vpath, "mcmerge", NULL);
        if (merge_file_fd == -1)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot create temporary merge file\n%s"),
                     unix_error_string (errno));
            return TRUE;
        }

        merge_file = fdopen (merge_file_fd, "w");

        switch (hunk)
        {
        case DIFF_DEL:
            if (source_diff == DIFF_RIGHT)
                dview_add_hunk (dview, merge_file, *dest_from, *source_from, *source_to,
                                source_diff, dest_diff);
            else
                dview_remove_hunk (dview, merge_file, *dest_from, *dest_to, dest_diff);
            break;
        case DIFF_ADD:
            if (source_diff == DIFF_RIGHT)
                dview_remove_hunk (dview, merge_file, *dest_from, *dest_to, source_diff);
            else
                dview_add_hunk (dview, merge_file, *dest_from, *source_from, to2, dest_diff,
                                source_diff);
            break;
        case DIFF_CHG:
            dview_replace_hunk (dview, merge_file, *dest_from, *dest_to, *source_from, *source_to,
                                source_diff, dest_diff);
            break;
        }
        fflush (merge_file);
        fclose (merge_file);
        {
            int res;

            res = rewrite_backup_content (merge_file_name_vpath, dview->file[source_diff]);
            (void) res;
        }
        mc_unlink (merge_file_name_vpath);
        vfs_path_free (merge_file_name_vpath);
    }

    mc_event_raise (MCEVENT_GROUP_DIFFVIEWER, "redo", dview, NULL, error);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_diffviewer_redo_diff (WDiff * dview)
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
        mc_diffviewer_file_reset (f[DIFF_LEFT]);
        mc_diffviewer_file_reset (f[DIFF_RIGHT]);
    }

    ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    ndiff =
        mc_diffviewer_execute (dview->args, extra, dview->file[DIFF_LEFT], dview->file[DIFF_RIGHT],
                               ops);
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
        mc_diffviewer_file_trunc (f[DIFF_LEFT]);
        mc_diffviewer_file_trunc (f[DIFF_RIGHT]);
    }

    if (dview->dsrc == DATA_SRC_MEM && HDIFF_ENABLE)
    {
        dview->hdiff = g_ptr_array_new ();
        if (dview->hdiff != NULL)
        {
            size_t i;

            for (i = 0; i < dview->a[DIFF_LEFT]->len; i++)
            {
                GArray *h = NULL;
                const DIFFLN *p;
                const DIFFLN *q;

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
