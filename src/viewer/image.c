/*
   Internal file viewer for the Midnight Commander
   Function for inline image preview (iTerm2 inline images protocol)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

// Do not try to preview files bigger than this
#define IMAGE_MAX_SIZE (16 * 1024 * 1024)

// Max size of a base64 payload in one kitty graphics escape code
#define KITTY_CHUNK_SIZE 4096

// Delete all images and free their data
#define KITTY_DELETE_ALL "\033_Ga=d,d=A,q=2\033\\"

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static mcview_image_proto_t
mcview_image_detect_protocol (void)
{
    const char *setting, *term, *term_program, *lc_terminal;

    // multiplexers need passthrough, which is not supported
    if (getenv ("TMUX") != NULL || getenv ("STY") != NULL)
        return MCVIEW_IMAGE_NONE;

    setting = (mcview_image_protocol != NULL) ? mcview_image_protocol : "auto";
    if (g_ascii_strcasecmp (setting, "none") == 0)
        return MCVIEW_IMAGE_NONE;
    if (g_ascii_strcasecmp (setting, "iterm2") == 0)
        return MCVIEW_IMAGE_ITERM2;
    if (g_ascii_strcasecmp (setting, "kitty") == 0)
        return MCVIEW_IMAGE_KITTY;
    // anything else, including "auto", means autodetection

    term = getenv ("TERM");
    term_program = getenv ("TERM_PROGRAM");
    lc_terminal = getenv ("LC_TERMINAL");  // survives ssh

    // prefer iTerm2 protocol: the terminal decodes all formats itself
    if (g_strcmp0 (term_program, "iTerm.app") == 0 || g_strcmp0 (lc_terminal, "iTerm2") == 0
        || g_strcmp0 (term_program, "WezTerm") == 0)
        return MCVIEW_IMAGE_ITERM2;

    if (getenv ("KITTY_WINDOW_ID") != NULL || g_strcmp0 (term, "xterm-kitty") == 0
        || g_strcmp0 (term_program, "ghostty") == 0)
        return MCVIEW_IMAGE_KITTY;

    return MCVIEW_IMAGE_NONE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_image_sniff (const unsigned char *d, size_t len)
{
    if (len >= 8 && memcmp (d, "\x89PNG\r\n\x1a\n", 8) == 0)
        return TRUE;
    if (len >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)  // JPEG
        return TRUE;
    if (len >= 6 && (memcmp (d, "GIF87a", 6) == 0 || memcmp (d, "GIF89a", 6) == 0))
        return TRUE;
    if (len >= 12 && memcmp (d, "RIFF", 4) == 0 && memcmp (d + 8, "WEBP", 4) == 0)
        return TRUE;
    if (len >= 4 && (memcmp (d, "II*\0", 4) == 0 || memcmp (d, "MM\0*", 4) == 0))  // TIFF
        return TRUE;
    if (len >= 26 && d[0] == 'B' && d[1] == 'M')  // BMP: check reserved fields
        return d[6] == 0 && d[7] == 0 && d[8] == 0 && d[9] == 0;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/** Get size of a PNG image from its IHDR chunk */

static gboolean
mcview_image_png_size (const unsigned char *d, size_t len, int *width, int *height)
{
    guint32 w, h;

    if (len < 24 || memcmp (d, "\x89PNG\r\n\x1a\n", 8) != 0 || memcmp (d + 12, "IHDR", 4) != 0)
        return FALSE;

    w = ((guint32) d[16] << 24) | ((guint32) d[17] << 16) | ((guint32) d[18] << 8) | d[19];
    h = ((guint32) d[20] << 24) | ((guint32) d[21] << 16) | ((guint32) d[22] << 8) | d[23];
    if (w == 0 || h == 0 || w > G_MAXINT || h > G_MAXINT)
        return FALSE;

    *width = (int) w;
    *height = (int) h;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * kitty scales the image to the given cell box ignoring the aspect ratio, so fit it ourselves.
 */

static void
mcview_image_fit (const WView *view, int *cols, int *lines)
{
    const WRect *r = &view->data_area;
    struct winsize ws;
    double cw = 1.0, ch = 2.0;  // cell size; only the ratio matters. Typical cell is 1:2
    double box_w, box_h;

    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0 && ws.ws_row != 0
        && ws.ws_xpixel != 0 && ws.ws_ypixel != 0)
    {
        cw = (double) ws.ws_xpixel / ws.ws_col;
        ch = (double) ws.ws_ypixel / ws.ws_row;
    }

    box_w = r->cols * cw;
    box_h = r->lines * ch;

    if ((double) view->image_width * box_h > (double) view->image_height * box_w)
    {
        // width is the limit
        *cols = r->cols;
        *lines = (int) (view->image_height * box_w / view->image_width / ch + 0.5);
    }
    else
    {
        *lines = r->lines;
        *cols = (int) (view->image_width * box_h / view->image_height / cw + 0.5);
    }

    *cols = CLAMP (*cols, 1, r->cols);
    *lines = CLAMP (*lines, 1, r->lines);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_image_write_all (const char *s, size_t len)
{
    while (len != 0)
    {
        ssize_t n;

        n = write (STDOUT_FILENO, s, len);
        if (n < 0)
            return;
        s += n;
        len -= (size_t) n;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Try to load the current file as an image for preview. On success view->image_b64 is set.
 */

void
mcview_image_load (WView *view)
{
    vfs_path_t *vpath = view->filename_vpath;
    mcview_image_proto_t proto;
    unsigned char *data;
    struct stat st;
    size_t got = 0;
    int fd;

    MC_PTR_FREE (view->image_b64);
    view->image_proto = MCVIEW_IMAGE_NONE;

    if (!mcview_is_in_panel (view) || vpath == NULL)
        return;

    proto = mcview_image_detect_protocol ();
    if (proto == MCVIEW_IMAGE_NONE)
        return;

    fd = mc_open (vpath, O_RDONLY | O_NONBLOCK);
    if (fd == -1)
        return;

    if (mc_fstat (fd, &st) == -1 || !S_ISREG (st.st_mode) || st.st_size <= 0
        || st.st_size > IMAGE_MAX_SIZE)
    {
        mc_close (fd);
        return;
    }

    data = g_try_malloc ((gsize) st.st_size);
    if (data != NULL)
    {
        while (got < (size_t) st.st_size)
        {
            ssize_t n;

            n = mc_read (fd, data + got, (size_t) st.st_size - got);
            if (n <= 0)
                break;
            got += (size_t) n;
        }

        if (got == (size_t) st.st_size && mcview_image_sniff (data, got))
        {
            // kitty can show PNG only
            if (proto == MCVIEW_IMAGE_ITERM2
                || mcview_image_png_size (data, got, &view->image_width, &view->image_height))
            {
                view->image_b64 = g_base64_encode (data, got);
                view->image_proto = proto;
            }
        }

        g_free (data);
    }

    mc_close (fd);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_image_active (const WView *view)
{
    return view->image_b64 != NULL && !view->mode_flags.hex;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Draw the image into the data area. Must be called after the area was cleaned.
 */

void
mcview_image_draw (WView *view)
{
    const WRect *r = &view->data_area;
    const Widget *w = WIDGET (view);
    GString *s;

    if (r->lines <= 0 || r->cols <= 0)
        return;

    // make curses output the cleaned area before we write over it behind its back
    tty_refresh ();

    s = g_string_sized_new (strlen (view->image_b64) + 256);
    // save cursor, move to the data area
    g_string_append_printf (s, "\0337\033[%d;%dH", w->rect.y + r->y + 1, w->rect.x + r->x + 1);

    if (view->image_proto == MCVIEW_IMAGE_KITTY)
    {
        const char *p = view->image_b64;
        size_t left = strlen (p);
        int cols, lines;

        mcview_image_fit (view, &cols, &lines);

        // don't pile up copies of the same image on repaints
        g_string_append (s, KITTY_DELETE_ALL);

        // transmit and display (a=T) PNG (f=100), don't move cursor (C=1), no responses (q=2)
        while (left != 0)
        {
            size_t n = MIN (left, KITTY_CHUNK_SIZE);

            if (p == view->image_b64)
                g_string_append_printf (s, "\033_Ga=T,f=100,c=%d,r=%d,C=1,q=2,m=%d;", cols, lines,
                                        left > n ? 1 : 0);
            else
                g_string_append_printf (s, "\033_Gq=2,m=%d;", left > n ? 1 : 0);

            g_string_append_len (s, p, (gssize) n);
            g_string_append (s, "\033\\");
            p += n;
            left -= n;
        }
    }
    else
    {
        g_string_append_printf (
            s, "\033]1337;File=inline=1;width=%d;height=%d;preserveAspectRatio=1:", r->cols,
            r->lines);
        g_string_append (s, view->image_b64);
        // BEL terminates OSC
        g_string_append_c (s, '\a');
    }

    // restore cursor
    g_string_append (s, "\0338");

    mcview_image_write_all (s->str, s->len);
    g_string_free (s, TRUE);

    view->image_shown = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove the image from the screen.
 *
 * kitty can delete images explicitly. iTerm2 keeps the image under any text written over it, and
 * curses doesn't know about the image at all, so wipe the terminal screen and make curses repaint
 * everything. If @redraw is FALSE, the caller is going to repaint the dialog itself.
 */

void
mcview_image_forget (WView *view, gboolean redraw)
{
    Widget *owner;

    if (!view->image_shown)
        return;

    view->image_shown = FALSE;

    if (view->image_proto == MCVIEW_IMAGE_KITTY)
    {
        mcview_image_write_all (KITTY_DELETE_ALL, strlen (KITTY_DELETE_ALL));
        return;
    }

    mcview_image_write_all ("\033[0m\033[2J", 8);
    tty_clear_screen ();

    owner = WIDGET (WIDGET (view)->owner);
    if (redraw && owner != NULL)
        widget_draw (owner);
}

/* --------------------------------------------------------------------------------------------- */
