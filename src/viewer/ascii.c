/*
   Internal file viewer for the Midnight Commander
   Function for plain view

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
   Ilia Maslakov <il.smind@gmail.com>, 2009
   Rewritten almost from scratch by:
   Egmont Koblinger <egmont@gmail.com>, 2014

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

   ------------------------------------------------------------------------------------------------

   The viewer is implemented along the following design principles:

   Goals: Always display simple scripts, double wide (CJK), combining accents and spacing marks
   (often used e.g. in Devanagari) perfectly. Make the arrow keys always work correctly.

   Absolutely non-goal: RTL.

   Terminology:

   - A "paragraph" is the text between two adjacent newline characters. A "line" or "row" is a
   visual row on the screen. In wrap mode, the viewer formats a paragraph into one or more lines.

   - The Unicode glossary <http://www.unicode.org/glossary/> doesn't seem to have a notion of "base
   character followed by zero or more combining characters". The closest matches are "Combining
   Character Sequence" meaning a base character followed by one or more combining characters, or
   "Grapheme" which seems to exclude non-printable characters such as newline. In this file,
   "combining character sequence" (or any obvious abbreviation thereof) means a base character
   followed by zero or more (up to a current limit of 4) combining characters.

   ------------------------------------------------------------------------------------------------

   The parser-formatter is designed to be stateless across paragraphs. This is so that we can walk
   backwards without having to reparse the whole file (although we still need to reparse and
   reformat the whole paragraph, but it's a lot better). This principle needs to be changed if we
   ever get to address tickets 1849/2977, but then we can still store (for efficiency) the parser
   state at the beginning of the paragraph, and safely walk backwards if we don't cross an escape
   character.

   The parser-formatter, however, definitely needs to carry a state across lines. Currently this
   state contains:

   - The logical column (as if we didn't wrap). This is used for handling TAB characters after a
   wordwrap consistently with less.

   - Whether the last nroff character was bold or underlined. This is used for displaying the
   ambiguous _\b_ sequence consistently with less.

   - Whether the desired way of displaying a lonely combining accent or spacing mark is to place it
   over a dotted circle (we do this at the beginning of the paragraph of after a TAB), or to ignore
   the combining char and show replacement char for the spacing mark (we do this if e.g. too many
   of these were encountered and hence we don't glue them with their base character).

   - (This state needs to be expanded if e.g. we decide to print verbose replacement characters
   (e.g. "<U+0080>") and allow these to wrap around lines.)

   The state also contains the file offset, as it doesn't make sense to ever know the state without
   knowing the corresponding offset.

   The state depends on various settings (viewer width, encoding, nroff mode, charwrap or wordwrap
   mode (if we'll have that one day) etc.), needs to be recomputed if any of these changes.

   Walking forwards is usually relatively easy both in the file and on the screen. Walking
   backwards within a paragraph would only be possible in some special cases and even then it would
   be painful, so we always walk back to the beginning of the paragraph and reparse-reformat from
   there.

   (Walking back within a line in the file would have at least the following difficulties: handling
   the parser state; processing invalid UTF-8; processing invalid nroff (e.g. what is "_\bA\bA"?).
   Walking back on the display: we wouldn't know where to display the last line of a paragraph, or
   where to display a line if its following line starts with a wide (CJK or Tab) character. Long
   story short: just forget this approach.)

   Most important variables:

   - dpy_start: Both in unwrap and wrap modes this points to the beginning of the topmost displayed
   paragraph.

   - dpy_text_column: Only in unwrap mode, an additional horizontal scroll.

   - dpy_paragraph_skip_lines: Only in wrap mode, an additional vertical scroll (the number of
   lines that are scrolled off at the top from the topmost paragraph).

   - dpy_state_top: Only in wrap mode, the offset and parser-formatter state at the line where
   displaying the file begins is cached here.

   - dpy_wrap_dirty: If some parameter has changed that makes it necessary to reparse-redisplay the
   topmost paragraph.

   In wrap mode, the three variables "dpy_start", "dpy_paragraph_skip_lines" and "dpy_state_top"
   are kept consistent. Think of the first two as the ones describing the position, and the third
   as a cached value for better performance so that we don't need to wrap the invisible beginning
   of the topmost paragraph over and over again. The third value needs to be recomputed each time a
   parameter that influences parsing or displaying the file (e.g. width of screen, encoding, nroff
   mode) changes, this is signaled by "dpy_wrap_dirty" to force recomputing "dpy_state_top" (and
   clamp "dpy_paragraph_skip_lines" if necessary).

   ------------------------------------------------------------------------------------------------

   Help integration

   I'm planning to port the help viewer to this codebase.

   Splitting at sections would still happen in the help viewer. It would either copy a section, or
   set force_max and a similar force_min to limit displaying to one section only.

   Parsing the help format would go next to the nroff parser. The colors, alternate character set,
   and emitting the version number would go to the "state". (The version number would be
   implemented by emitting remaining characters of a buffer in the "state" one by one, without
   advancing in the file position.)

   The active link would be drawn similarly to the search highlight. Other than that, the viewer
   wouldn't care about links (except for their color). help.c would keep track of which one is
   highlighted, how to advance to the next/prev on an arrow, how the scroll offset needs to be
   adjusted when moving, etc.

   Add wrapping at word boundaries to where wrapping at char boundaries happens now.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/util.h"           /* is_printable() */
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/setup.h"          /* option_tab_spacing */

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* The Unicode standard recommends that lonely combining characters are printed over a dotted
 * circle. If the terminal is not UTF-8, this will be replaced by a dot anyway. */
#define BASE_CHARACTER_FOR_LONELY_COMBINING 0x25CC      /* dotted circle */
#define MAX_COMBINING_CHARS 4   /* both slang and ncurses support exactly 4 */

/* I think anything other than space (e.g. arrows) just introduce visual clutter without actually
 * adding value. */
#define PARTIAL_CJK_AT_LEFT_MARGIN  ' '
#define PARTIAL_CJK_AT_RIGHT_MARGIN ' '

/*
 * Wrap mode: This is for safety so that jumping to the end of file (which already includes
 * scrolling back by a page) and then walking backwards is reasonably fast, even if the file is
 * extremely large and consists of maybe full zeros or something like that. If there's no newline
 * found within this limit, just start displaying from there and see what happens. We might get
 * some displaying parameters (most importantly the columns) incorrect, but at least will show the
 * file without spinning the CPU for ages. When scrolling back to that point, the user might see a
 * garbled first line (even starting with an invalid partial UTF-8), but then walking back by yet
 * another line should fix it.
 *
 * Unwrap mode: This is not used, we wouldn't be able to do anything reasonable without walking
 * back a whole paragraph (well, view->data_area.height paragraphs actually).
 */
#define MAX_BACKWARDS_WALK_IN_PARAGRAPH (100 * 1000)

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* TODO: These methods shouldn't be necessary, see ticket 3257 */

static int
mcview_wcwidth (const WView *view, int c)
{
#ifdef HAVE_CHARSET
    if (view->utf8)
    {
        if (g_unichar_iswide (c))
            return 2;
        if (g_unichar_iszerowidth (c))
            return 0;
    }
#else
    (void) view;
    (void) c;
#endif /* HAVE_CHARSET */
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_ismark (const WView *view, int c)
{
#ifdef HAVE_CHARSET
    if (view->utf8)
        return g_unichar_ismark (c);
#else
    (void) view;
    (void) c;
#endif /* HAVE_CHARSET */
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* actually is_non_spacing_mark_or_enclosing_mark */
static gboolean
mcview_is_non_spacing_mark (const WView *view, int c)
{
#ifdef HAVE_CHARSET
    if (view->utf8)
    {
        GUnicodeType type;

        type = g_unichar_type (c);

        return type == G_UNICODE_NON_SPACING_MARK || type == G_UNICODE_ENCLOSING_MARK;
    }
#else
    (void) view;
    (void) c;
#endif /* HAVE_CHARSET */
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static gboolean
mcview_is_spacing_mark (const WView *view, int c)
{
#ifdef HAVE_CHARSET
    if (view->utf8)
        return g_unichar_type (c) == G_UNICODE_SPACING_MARK;
#else
    (void) view;
    (void) c;
#endif /* HAVE_CHARSET */
    return FALSE;
}
#endif /* 0 */

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_isprint (const WView *view, int c)
{
#ifdef HAVE_CHARSET
    if (!view->utf8)
        c = convert_from_8bit_to_utf_c ((unsigned char) c, view->converter);
    return g_unichar_isprint (c);
#else
    (void) view;
    /* TODO this is very-very buggy by design: ticket 3257 comments 0-1 */
    return is_printable (c);
#endif /* HAVE_CHARSET */
}

/* --------------------------------------------------------------------------------------------- */

static int
mcview_char_display (const WView *view, int c, char *s)
{
#ifdef HAVE_CHARSET
    if (mc_global.utf8_display)
    {
        if (!view->utf8)
            c = convert_from_8bit_to_utf_c ((unsigned char) c, view->converter);
        if (!g_unichar_isprint (c))
            c = '.';
        return g_unichar_to_utf8 (c, s);
    }
    if (view->utf8)
    {
        if (g_unichar_iswide (c))
        {
            s[0] = s[1] = '.';
            return 2;
        }
        if (g_unichar_iszerowidth (c))
            return 0;
        /* TODO the is_printable check below will be broken for this */
        c = convert_from_utf_to_current_c (c, view->converter);
    }
    else
    {
        /* TODO the is_printable check below will be broken for this */
        c = convert_to_display_c (c);
    }
#else
    (void) view;
#endif /* HAVE_CHARSET */
    /* TODO this is very-very buggy by design: ticket 3257 comments 0-1 */
    if (!is_printable (c))
        c = '.';
    *s = c;
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Just for convenience, a common interface in front of mcview_get_utf and mcview_get_byte, so that
 * the caller doesn't have to care about utf8 vs 8-bit modes.
 *
 * Normally: stores c, updates state, returns TRUE.
 * At EOF: state is unchanged, c is undefined, returns FALSE.
 *
 * Just as with mcview_get_utf(), invalid UTF-8 is reported using negative integers.
 *
 * Also, temporary hack: handle force_max here.
 * TODO: move it to lower layers (datasource.c)?
 */
static gboolean
mcview_get_next_char (WView *view, mcview_state_machine_t *state, int *c)
{
    /* Pretend EOF if we reached force_max */
    if (view->force_max >= 0 && state->offset >= view->force_max)
        return FALSE;

#ifdef HAVE_CHARSET
    if (view->utf8)
    {
        int char_length = 0;

        if (!mcview_get_utf (view, state->offset, c, &char_length))
            return FALSE;
        /* Pretend EOF if we crossed force_max */
        if (view->force_max >= 0 && state->offset + char_length > view->force_max)
            return FALSE;

        state->offset += char_length;
        return TRUE;
    }
#endif /* HAVE_CHARSET */
    if (!mcview_get_byte (view, state->offset, c))
        return FALSE;
    state->offset++;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This function parses the next nroff character and gives it to you along with its desired color,
 * so you never have to care about nroff again.
 *
 * The nroff mode does the backspace trick for every single character (Unicode codepoint). At least
 * that's what the GNU groff 1.22 package produces, and that's what less 458 expects. For
 * double-wide characters (CJK), still only a single backspace is emitted. For combining accents
 * and such, the print-backspace-print step is repeated for the base character and then for each
 * accent separately.
 *
 * So, the right place for this layer is after the bytes are interpreted in UTF-8, but before
 * joining a base character with its combining accents.
 *
 * Normally: stores c and color, updates state, returns TRUE.
 * At EOF: state is unchanged, c and color are undefined, returns FALSE.
 *
 * color can be null if the caller doesn't care.
 */
static gboolean
mcview_get_next_maybe_nroff_char (WView *view, mcview_state_machine_t *state, int *c, int *color)
{
    mcview_state_machine_t state_after_nroff;
    int c2, c3;

    if (color != NULL)
        *color = VIEW_NORMAL_COLOR;

    if (!view->mode_flags.nroff)
        return mcview_get_next_char (view, state, c);

    if (!mcview_get_next_char (view, state, c))
        return FALSE;
    /* Don't allow nroff formatting around CR, LF, TAB or other special chars */
    if (!mcview_isprint (view, *c))
        return TRUE;

    state_after_nroff = *state;

    if (!mcview_get_next_char (view, &state_after_nroff, &c2))
        return TRUE;
    if (c2 != '\b')
        return TRUE;

    if (!mcview_get_next_char (view, &state_after_nroff, &c3))
        return TRUE;
    if (!mcview_isprint (view, c3))
        return TRUE;

    if (*c == '_' && c3 == '_')
    {
        *state = state_after_nroff;
        if (color != NULL)
            *color =
                state->nroff_underscore_is_underlined ? VIEW_UNDERLINED_COLOR : VIEW_BOLD_COLOR;
    }
    else if (*c == c3)
    {
        *state = state_after_nroff;
        state->nroff_underscore_is_underlined = FALSE;
        if (color != NULL)
            *color = VIEW_BOLD_COLOR;
    }
    else if (*c == '_')
    {
        *c = c3;
        *state = state_after_nroff;
        state->nroff_underscore_is_underlined = TRUE;
        if (color != NULL)
            *color = VIEW_UNDERLINED_COLOR;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get one base character, along with its combining or spacing mark characters.
 *
 * (A spacing mark is a character that extends the base character's width 1 into a combined
 * character of width 2, yet these two character cells should not be separated. E.g. Devanagari
 * <U+0939><U+094B>.)
 *
 * This method exists mainly for two reasons. One is to be able to tell if we fit on the current
 * line or need to wrap to the next one. The other is that both slang and ncurses seem to require
 * that the character and its combining marks are printed in a single call (or is it just a
 * limitation of mc's wrapper to them?).
 *
 * For convenience, this method takes care of converting CR or CR+LF into LF.
 * TODO this should probably happen later, when displaying the file?
 *
 * Normally: stores cs and color, updates state, returns >= 1 (entries in cs).
 * At EOF: state is unchanged, cs and color are undefined, returns 0.
 *
 * @param view ...
 * @param state the parser-formatter state machine's state, updated
 * @param cs store the characters here
 * @param clen the room available in cs (that is, at most clen-1 combining marks are allowed), must
 *   be at least 2
 * @param color if non-NULL, store the color here, taken from the first codepoint's color
 * @return the number of entries placed in cs, or 0 on EOF
 */
static int
mcview_next_combining_char_sequence (WView *view, mcview_state_machine_t *state, int *cs,
                                     int clen, int *color)
{
    int i = 1;

    if (!mcview_get_next_maybe_nroff_char (view, state, cs, color))
        return 0;

    /* Process \r and \r\n newlines. */
    if (cs[0] == '\r')
    {
        int cnext;

        mcview_state_machine_t state_after_crlf = *state;
        if (mcview_get_next_maybe_nroff_char (view, &state_after_crlf, &cnext, NULL)
            && cnext == '\n')
            *state = state_after_crlf;
        cs[0] = '\n';
        return 1;
    }

    /* We don't want combining over non-printable characters. This includes '\n' and '\t' too. */
    if (!mcview_isprint (view, cs[0]))
        return 1;

    if (mcview_ismark (view, cs[0]))
    {
        if (!state->print_lonely_combining)
        {
            /* First character is combining. Either just return it, ... */
            return 1;
        }
        else
        {
            /* or place this (and subsequent combining ones) over a dotted circle. */
            cs[1] = cs[0];
            cs[0] = BASE_CHARACTER_FOR_LONELY_COMBINING;
            i = 2;
        }
    }

    if (mcview_wcwidth (view, cs[0]) == 2)
    {
        /* Don't allow combining or spacing mark for wide characters, is this okay? */
        return 1;
    }

    /* Look for more combining chars. Either at most clen-1 zero-width combining chars,
     * or at most 1 spacing mark. Is this logic correct? */
    for (; i < clen; i++)
    {
        mcview_state_machine_t state_after_combining;

        state_after_combining = *state;
        if (!mcview_get_next_maybe_nroff_char (view, &state_after_combining, &cs[i], NULL))
            return i;
        if (!mcview_ismark (view, cs[i]) || !mcview_isprint (view, cs[i]))
            return i;
        if (g_unichar_type (cs[i]) == G_UNICODE_SPACING_MARK)
        {
            /* Only allow as the first combining char. Stop processing in either case. */
            if (i == 1)
            {
                *state = state_after_combining;
                i++;
            }
            return i;
        }
        *state = state_after_combining;
    }
    return i;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Parse, format and possibly display one visual line of text.
 *
 * Formatting starts at the given "state" (which encodes the file offset and parser and formatter's
 * internal state). In unwrap mode, this should point to the beginning of the paragraph with the
 * default state, the additional horizontal scrolling is added here. In wrap mode, this should
 * point to the beginning of the line, with the proper state at that point.
 *
 * In wrap mode, if a line ends in a newline, it is consumed, even if it's exactly at the right
 * edge. In unwrap mode, the whole remaining line, including the newline is consumed. Displaying
 * the next line should start at "state"'s new value, or if we displayed the bottom line then
 * state->offset tells the file offset to be shown in the top bar.
 *
 * If "row" is offscreen, don't actually display the line but still update "state" and return the
 * proper value. This is used by mcview_wrap_move_down to advance in the file.
 *
 * @param view ...
 * @param state the parser-formatter state machine's state, updated
 * @param row print to this row
 * @param paragraph_ended store TRUE if paragraph ended by newline or EOF, FALSE if wraps to next
 *   line
 * @param linewidth store the width of the line here
 * @return the number of rows, that is, 0 if we were already at EOF, otherwise 1
 */
static int
mcview_display_line (WView *view, mcview_state_machine_t *state, int row,
                     gboolean *paragraph_ended, off_t *linewidth)
{
    const WRect *r = &view->data_area;
    off_t dpy_text_column = view->mode_flags.wrap ? 0 : view->dpy_text_column;
    int col = 0;
    int cs[1 + MAX_COMBINING_CHARS];
    char str[(1 + MAX_COMBINING_CHARS) * UTF8_CHAR_LEN + 1];
    int i, j;

    if (paragraph_ended != NULL)
        *paragraph_ended = TRUE;

    if (!view->mode_flags.wrap && (row < 0 || row >= r->lines) && linewidth == NULL)
    {
        /* Optimization: Fast forward to the end of the line, rather than carefully
         * parsing and then not actually displaying it. */
        off_t eol;
        int retval;

        eol = mcview_eol (view, state->offset);
        retval = (eol > state->offset) ? 1 : 0;

        mcview_state_machine_init (state, eol);
        return retval;
    }

    while (TRUE)
    {
        int charwidth = 0;
        mcview_state_machine_t state_saved;
        int n;
        int color;

        state_saved = *state;
        n = mcview_next_combining_char_sequence (view, state, cs, 1 + MAX_COMBINING_CHARS, &color);
        if (n == 0)
        {
            if (linewidth != NULL)
                *linewidth = col;
            return (col > 0) ? 1 : 0;
        }

        if (view->search_start <= state->offset && state->offset < view->search_end)
            color = VIEW_SELECTED_COLOR;

        if (cs[0] == '\n')
        {
            /* New line: reset all formatting state for the next paragraph. */
            mcview_state_machine_init (state, state->offset);
            if (linewidth != NULL)
                *linewidth = col;
            return 1;
        }

        if (mcview_is_non_spacing_mark (view, cs[0]))
        {
            /* Lonely combining character. Probably leftover after too many combining chars. Just ignore. */
            continue;
        }

        /* Nonprintable, or lonely spacing mark */
        if ((!mcview_isprint (view, cs[0]) || mcview_ismark (view, cs[0])) && cs[0] != '\t')
            cs[0] = '.';

        for (i = 0; i < n; i++)
            charwidth += mcview_wcwidth (view, cs[i]);

        /* Adjust the width for TAB. It's handled below along with the normal characters,
         * so that it's wrapped consistently with them, and is painted with the proper
         * attributes (although currently it can't have a special color). */
        if (cs[0] == '\t')
        {
            charwidth = option_tab_spacing - state->unwrapped_column % option_tab_spacing;
            state->print_lonely_combining = TRUE;
        }
        else
            state->print_lonely_combining = FALSE;

        /* In wrap mode only: We're done with this row if the character sequence wouldn't fit.
         * Except if at the first column, because then it wouldn't fit in the next row either.
         * In this extreme case let the unwrapped code below do its best to display it. */
        if (view->mode_flags.wrap && (off_t) col + charwidth > dpy_text_column + (off_t) r->cols
            && col > 0)
        {
            *state = state_saved;
            if (paragraph_ended != NULL)
                *paragraph_ended = FALSE;
            if (linewidth != NULL)
                *linewidth = col;
            return 1;
        }

        /* Display, unless outside of the viewport. */
        if (row >= 0 && row < r->lines)
        {
            if ((off_t) col >= dpy_text_column &&
                (off_t) col + charwidth <= dpy_text_column + (off_t) r->cols)
            {
                /* The combining character sequence fits entirely in the viewport. Print it. */
                tty_setcolor (color);
                widget_gotoyx (view, r->y + row, r->x + ((off_t) col - dpy_text_column));
                if (cs[0] == '\t')
                {
                    for (i = 0; i < charwidth; i++)
                        tty_print_char (' ');
                }
                else
                {
                    j = 0;
                    for (i = 0; i < n; i++)
                        j += mcview_char_display (view, cs[i], str + j);
                    str[j] = '\0';
                    /* This is probably a bug in our tty layer, but tty_print_string
                     * normalizes the string, whereas tty_printf doesn't. Don't normalize,
                     * since we handle combining characters ourselves correctly, it's
                     * better if they are copy-pasted correctly. Ticket 3255. */
                    tty_printf ("%s", str);
                }
            }
            else if ((off_t) col < dpy_text_column && (off_t) col + charwidth > dpy_text_column)
            {
                /* The combining character sequence would cross the left edge of the viewport.
                 * This cannot happen with wrap mode. Print replacement character(s),
                 * or spaces with the correct attributes for partial Tabs. */
                tty_setcolor (color);
                for (i = dpy_text_column;
                     i < (off_t) col + charwidth && i < dpy_text_column + (off_t) r->cols; i++)
                {
                    widget_gotoyx (view, r->y + row, r->x + (i - dpy_text_column));
                    tty_print_anychar ((cs[0] == '\t') ? ' ' : PARTIAL_CJK_AT_LEFT_MARGIN);
                }
            }
            else if ((off_t) col < dpy_text_column + (off_t) r->cols &&
                     (off_t) col + charwidth > dpy_text_column + (off_t) r->cols)
            {
                /* The combining character sequence would cross the right edge of the viewport
                 * and we're not wrapping. Print replacement character(s),
                 * or spaces with the correct attributes for partial Tabs. */
                tty_setcolor (color);
                for (i = col; i < dpy_text_column + (off_t) r->cols; i++)
                {
                    widget_gotoyx (view, r->y + row, r->x + (i - dpy_text_column));
                    tty_print_anychar ((cs[0] == '\t') ? ' ' : PARTIAL_CJK_AT_RIGHT_MARGIN);
                }
            }
        }

        col += charwidth;
        state->unwrapped_column += charwidth;

        if (!view->mode_flags.wrap && (off_t) col >= dpy_text_column + (off_t) r->cols
            && linewidth == NULL)
        {
            /* Optimization: Fast forward to the end of the line, rather than carefully
             * parsing and then not actually displaying it. */
            off_t eol;

            eol = mcview_eol (view, state->offset);
            mcview_state_machine_init (state, eol);
            return 1;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Parse, format and possibly display one paragraph (perhaps not from the beginning).
 *
 * Formatting starts at the given "state" (which encodes the file offset and parser and formatter's
 * internal state). In unwrap mode, this should point to the beginning of the paragraph with the
 * default state, the additional horizontal scrolling is added here. In wrap mode, this may point
 * to the beginning of the line within a paragraph (to display the partial paragraph at the top),
 * with the proper state at that point.
 *
 * Displaying the next paragraph should start at "state"'s new value, or if we displayed the bottom
 * line then state->offset tells the file offset to be shown in the top bar.
 *
 * If "row" is negative, don't display the first abs(row) lines and display the rest from the top.
 * This was a nice idea but it's now unused :)
 *
 * If "row" is too large, don't display the paragraph at all but still return the number of lines.
 * This is used when moving upwards.
 *
 * @param view ...
 * @param state the parser-formatter state machine's state, updated
 * @param row print starting at this row
 * @return the number of rows the paragraphs is wrapped to, that is, 0 if we were already at EOF,
 *   otherwise 1 in unwrap mode, >= 1 in wrap mode. We stop when reaching the bottom of the
 *   viewport, it's not counted how many more lines the paragraph would occupy
 */
static int
mcview_display_paragraph (WView *view, mcview_state_machine_t *state, int row)
{
    int lines = 0;

    while (TRUE)
    {
        gboolean paragraph_ended;

        lines += mcview_display_line (view, state, row, &paragraph_ended, NULL);
        if (paragraph_ended)
            return lines;

        if (row < view->data_area.lines)
        {
            row++;
            /* stop if bottom of screen reached */
            if (row >= view->data_area.lines)
                return lines;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Recompute dpy_state_top from dpy_start and dpy_paragraph_skip_lines. Clamp
 * dpy_paragraph_skip_lines if necessary.
 *
 * This method should be called in wrap mode after changing one of the parsing or formatting
 * properties (e.g. window width, encoding, nroff), or when switching to wrap mode from unwrap or
 * hex.
 *
 * If we stayed within the same paragraph then try to keep the vertical offset within that
 * paragraph as well. It might happen though that the paragraph became shorter than our desired
 * vertical position, in that case move to its last row.
 */
static void
mcview_wrap_fixup (WView *view)
{
    int lines = view->dpy_paragraph_skip_lines;

    if (!view->dpy_wrap_dirty)
        return;
    view->dpy_wrap_dirty = FALSE;

    view->dpy_paragraph_skip_lines = 0;
    mcview_state_machine_init (&view->dpy_state_top, view->dpy_start);

    while (lines-- != 0)
    {
        mcview_state_machine_t state_prev;
        gboolean paragraph_ended;

        state_prev = view->dpy_state_top;
        if (mcview_display_line (view, &view->dpy_state_top, -1, &paragraph_ended, NULL) == 0)
            break;
        if (paragraph_ended)
        {
            view->dpy_state_top = state_prev;
            break;
        }
        view->dpy_paragraph_skip_lines++;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * In both wrap and unwrap modes, dpy_start points to the beginning of the paragraph.
 *
 * In unwrap mode, start displaying from this position, probably applying an additional horizontal
 * scroll.
 *
 * In wrap mode, an additional dpy_paragraph_skip_lines lines are skipped from the top of this
 * paragraph. dpy_state_top contains the position and parser-formatter state corresponding to the
 * top left corner so we can just start rendering from here. Unless dpy_wrap_dirty is set in which
 * case dpy_state_top is invalid and we need to recompute first.
 */
void
mcview_display_text (WView *view)
{
    const WRect *r = &view->data_area;
    int row;
    mcview_state_machine_t state;
    gboolean again;

    do
    {
        int n;

        again = FALSE;

        mcview_display_clean (view);
        mcview_display_ruler (view);

        if (!view->mode_flags.wrap)
            mcview_state_machine_init (&state, view->dpy_start);
        else
        {
            mcview_wrap_fixup (view);
            state = view->dpy_state_top;
        }

        for (row = 0; row < r->lines; row += n)
        {
            n = mcview_display_paragraph (view, &state, row);
            if (n == 0)
            {
                /* In the rare case that displaying didn't start at the beginning
                 * of the file, yet there are some empty lines at the bottom,
                 * scroll the file and display again. This happens when e.g. the
                 * window is made bigger, or the file becomes shorter due to
                 * charset change or enabling nroff. */
                if ((view->mode_flags.wrap ? view->dpy_state_top.offset : view->dpy_start) > 0)
                {
                    mcview_ascii_move_up (view, r->lines - row);
                    again = TRUE;
                }
                break;
            }
        }
    }
    while (again);

    view->dpy_end = state.offset;
    view->dpy_state_bottom = state;

    tty_setcolor (VIEW_NORMAL_COLOR);
    if (mcview_show_eof != NULL && mcview_show_eof[0] != '\0')
        while (row < r->lines)
        {
            widget_gotoyx (view, r->y + row, r->x);
            /* TODO: should make it no wider than the viewport */
            tty_print_string (mcview_show_eof);
            row++;
        }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Move down.
 *
 * It's very simple. Just invisibly format the next "lines" lines, carefully carrying the formatter
 * state in wrap mode. But before each step we need to check if we've already hit the end of the
 * file, in that case we can no longer move. This is done by walking from dpy_state_bottom.
 *
 * Note that this relies on mcview_display_text() setting dpy_state_bottom to its correct value
 * upon rendering the screen contents. So don't call this function from other functions (e.g. at
 * the bottom of mcview_ascii_move_up()) which invalidate this value.
 */
void
mcview_ascii_move_down (WView *view, off_t lines)
{
    while (lines-- != 0)
    {
        gboolean paragraph_ended;

        /* See if there's still data below the bottom line, by imaginarily displaying one
         * more line. This takes care of reading more data into growbuf, if required.
         * If the end position didn't advance, we're at EOF and hence bail out. */
        if (mcview_display_line (view, &view->dpy_state_bottom, -1, &paragraph_ended, NULL) == 0)
            break;

        /* Okay, there's enough data. Move by 1 row at the top, too. No need to check for
         * EOF, that can't happen. */
        if (!view->mode_flags.wrap)
        {
            view->dpy_start = mcview_eol (view, view->dpy_start);
            view->dpy_paragraph_skip_lines = 0;
            view->dpy_wrap_dirty = TRUE;
        }
        else
        {
            mcview_display_line (view, &view->dpy_state_top, -1, &paragraph_ended, NULL);
            if (!paragraph_ended)
                view->dpy_paragraph_skip_lines++;
            else
            {
                view->dpy_start = view->dpy_state_top.offset;
                view->dpy_paragraph_skip_lines = 0;
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Move up.
 *
 * Unwrap mode: Piece of cake. Wrap mode: If we'd walk back more than the current line offset
 * within the paragraph, we need to jump back to the previous paragraph and compute its height to
 * see if we start from that paragraph, and repeat this if necessary. Once we're within the desired
 * paragraph, we still need to format it from its beginning to know the state.
 *
 * See the top of this file for comments about MAX_BACKWARDS_WALK_IN_PARAGRAPH.
 *
 * force_max is a nice protection against the rare extreme case that the file underneath us
 * changes, we don't want to endlessly consume a file of maybe full of zeros upon moving upwards.
 */
void
mcview_ascii_move_up (WView *view, off_t lines)
{
    if (!view->mode_flags.wrap)
    {
        while (lines-- != 0)
            view->dpy_start = mcview_bol (view, view->dpy_start - 1, 0);
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
    }
    else
    {
        int i;

        while (lines > view->dpy_paragraph_skip_lines)
        {
            /* We need to go back to the previous paragraph. */
            if (view->dpy_start == 0)
            {
                /* Oops, we're already in the first paragraph. */
                view->dpy_paragraph_skip_lines = 0;
                mcview_state_machine_init (&view->dpy_state_top, 0);
                return;
            }
            lines -= view->dpy_paragraph_skip_lines;
            view->force_max = view->dpy_start;
            view->dpy_start =
                mcview_bol (view, view->dpy_start - 1,
                            view->dpy_start - MAX_BACKWARDS_WALK_IN_PARAGRAPH);
            mcview_state_machine_init (&view->dpy_state_top, view->dpy_start);
            /* This is a tricky way of denoting that we're at the end of the paragraph.
             * Normally we'd jump to the next paragraph and reset paragraph_skip_lines. But for
             * walking backwards this is exactly what we need. */
            view->dpy_paragraph_skip_lines =
                mcview_display_paragraph (view, &view->dpy_state_top, view->data_area.lines);
            view->force_max = -1;
        }

        /* Okay, we have have dpy_start pointing to the desired paragraph, and we still need to
         * walk back "lines" lines from the current "dpy_paragraph_skip_lines" offset. We can't do
         * that, so walk from the beginning of the paragraph. */
        mcview_state_machine_init (&view->dpy_state_top, view->dpy_start);
        view->dpy_paragraph_skip_lines -= lines;
        for (i = 0; i < view->dpy_paragraph_skip_lines; i++)
            mcview_display_line (view, &view->dpy_state_top, -1, NULL, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_ascii_moveto_bol (WView *view)
{
    if (!view->mode_flags.wrap)
        view->dpy_text_column = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_ascii_moveto_eol (WView *view)
{
    if (!view->mode_flags.wrap)
    {
        mcview_state_machine_t state;
        off_t linewidth;

        /* Get the width of the topmost paragraph. */
        mcview_state_machine_init (&state, view->dpy_start);
        mcview_display_line (view, &state, -1, NULL, &linewidth);
        view->dpy_text_column = DOZ (linewidth, (off_t) view->data_area.cols);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_state_machine_init (mcview_state_machine_t *state, off_t offset)
{
    memset (state, 0, sizeof (*state));
    state->offset = offset;
    state->print_lonely_combining = TRUE;
}

/* --------------------------------------------------------------------------------------------- */
