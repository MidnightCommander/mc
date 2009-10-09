/* Keyboard support routines.
    Interface functions.

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2008, 2009 Free Software Foundation, Inc.
   
   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   Slava Zanko <slavazanko@gmail.com>, 2009.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file color.c
 *  \brief Source: color setup
 */

#include <config.h>
#include <stdlib.h> /* for stupid exit() call */

#include <sys/time.h>
#include <sys/types.h>

#include "../src/global.h"
#include "../src/tty/key.h"
#include "../../src/tty/key-internal.h"
#include "../src/tty/keystruct.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static int seq_buffer[SEQ_BUFFER_LEN];
int *seq_append = NULL;


static key_def *this = NULL, *parent;
static struct timeval esctime = { -1, -1 };
static int lastnodelay = -1;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
get_key_code__getch (int no_delay)
{
    int c;
#if (defined(USE_NCURSES) || defined(USE_NCURSESW)) && defined(KEY_RESIZE)
    do {
        if (no_delay)
            tty_nodelay (TRUE);
        c = tty_lowlevel_getch ();
    } while (c == KEY_RESIZE);
#else
    if (no_delay)
        tty_nodelay (TRUE);

    c = tty_lowlevel_getch ();
#endif
    return c;
}

/* --------------------------------------------------------------------------------------------- */

static tty_key_t
get_key_code__parse_pending_keys (int no_delay)
{
    tty_key_t key = HOTKEY (KEY_M_NONE, 0);
    int d;

    d = *pending_keys++;

    while (TRUE) {
        if (*pending_keys == 0) {
            pending_keys = NULL;
            seq_append = NULL;
        }
        if ((d == ESC_CHAR) && (pending_keys != NULL)) {
            d = 0;
            key.modificator |= KEY_M_ALT;
            continue;
        }

        if ((d > 127 && d < 256) && use_8th_bit_as_meta) {
            key.modificator |= KEY_M_ALT;
            key.code = d & 0x7f;
            break;
        }

        key.code = d;           // FIXME: is this right? This just copy old behavior, but what about UTF-8?
        break;
    }
    return correct_key_code (key);
}

/* --------------------------------------------------------------------------------------------- */

tty_key_t
tty_key_parse_delay_and_make_esc_char (void)
{
    if (this != NULL && parent != NULL && parent->action == MCKEY_ESCAPE && old_esc_mode) {
        struct timeval current, timeout;

        if (esctime.tv_sec == -1)
            return HOTKEY(KEY_M_INVALID,0);
        GET_TIME (current);
        timeout.tv_sec = keyboard_key_timeout / 1000000 + esctime.tv_sec;
        timeout.tv_usec = keyboard_key_timeout % 1000000 + esctime.tv_usec;
        if (timeout.tv_usec > 1000000) {
            timeout.tv_usec -= 1000000;
            timeout.tv_sec++;
        }
        if (current.tv_sec < timeout.tv_sec)
            return HOTKEY(KEY_M_INVALID,0);
        if (current.tv_sec == timeout.tv_sec && current.tv_usec < timeout.tv_usec)
            return HOTKEY(KEY_M_INVALID,0);
        this = NULL;
        pending_keys = seq_append = NULL;
        return HOTKEY(KEY_M_NONE, W_ESC_CHAR);
    }
    return HOTKEY(KEY_M_INVALID,0);
}

/* --------------------------------------------------------------------------------------------- */

static int
tty_key_read_stdin_with_delay (void)
{
    fd_set Read_FD_Set;
    int c;
    struct timeval timeout;

    timeout.tv_sec = keyboard_key_timeout / 1000000;
    timeout.tv_usec = keyboard_key_timeout % 1000000;
    tty_nodelay (TRUE);
    FD_ZERO (&Read_FD_Set);
    FD_SET (input_fd, &Read_FD_Set);
    select (input_fd + 1, &Read_FD_Set, NULL, NULL, &timeout);
    c = tty_lowlevel_getch ();
    tty_nodelay (FALSE);
    return c;
}


/* --------------------------------------------------------------------------------------------- */

static gboolean
push_char (int c)
{
    gboolean ret = FALSE;

    if (seq_append == NULL)
        seq_append = seq_buffer;

    if (seq_append != &(seq_buffer[SEQ_BUFFER_LEN - 2])) {
        *(seq_append++) = c;
        *seq_append = 0;
        return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

tty_key_t
tty_key_parse (int no_delay)
{
    tty_key_t key = HOTKEY (KEY_M_NONE, 0);

    if (no_delay != lastnodelay) {
        this = NULL;
        lastnodelay = no_delay;
    }

  pend_send:
    if (pending_keys != NULL) {
        this = NULL;
        return get_key_code__parse_pending_keys (no_delay);
    }

  nodelay_try_again:
    key.code = get_key_code__getch (no_delay);

    if (no_delay) {
        tty_nodelay (FALSE);
        if (key.code == -1) {
            return tty_key_parse_delay_and_make_esc_char ();
        }
    } else if (key.code == -1) {
        /* Maybe we got an incomplete match.
           This we do only in delay mode, since otherwise
           tty_lowlevel_getch can return -1 at any time. */
        if (seq_append != NULL) {
            pending_keys = seq_buffer;
            goto pend_send;
        }
        this = NULL;
        return HOTKEY(KEY_M_INVALID,0);
    }

    /* Search the key on the root */
    if (!no_delay || this == NULL) {
        this = keys;
        parent = NULL;

        if ((key.code > 127 && key.code < 256) && use_8th_bit_as_meta) {
            key.code &= 0x7f;

            /* The first sequence defined starts with esc */
            parent = keys;
                this = keys->child;
        }
    }
    while (this != NULL) {
        if (key.code == this->ch) {
            if (this->child) {
                if (!push_char (key.code)) { // FIXME: push_char - WTF?
                    pending_keys = seq_buffer;
                    goto pend_send;
                }
                parent = this;
                this = this->child;
                if (parent->action == MCKEY_ESCAPE && old_esc_mode) {
                    if (no_delay) {
                        GET_TIME (esctime);
                        if (this == NULL) {
                            /* Shouldn't happen */
                            fputs ("Internal error\n", stderr);
                            exit (1); // FIXME: exit??? What about unclosed descriptors and unfreeing memory???
                        }
                        goto nodelay_try_again;
                    }
                    esctime.tv_sec = -1;
                    key.code = tty_key_read_stdin_with_delay ();
                    if (key.code == -1) {
                        pending_keys = seq_append = NULL;
                        this = NULL;
                        return HOTKEY(KEY_M_NONE, W_ESC_CHAR);
                    }
                } else {
                    if (no_delay)
                        goto nodelay_try_again;
                    key.code = tty_lowlevel_getch (); // FIXME: Hm... what we get at this point?
                }
            } else {
                /* We got a complete match, return and reset search */
                pending_keys = seq_append = NULL;
                key.code = this->code; // FIXME: Need to check this->code
                this = NULL;
                return correct_key_code (key); 
            }
        } else {
            if (this->next != NULL)
                this = this->next;
            else {
                if ((parent != NULL) && (parent->action == MCKEY_ESCAPE)) {
                    /* Convert escape-digits to F-keys */
                    if (g_ascii_isdigit (c))
                        c = KEY_F (c - '0');
                    else if (c == ' ')
                        c = ESC_CHAR;
                    else
                        c = ALT (c);

                    pending_keys = seq_append = NULL;
                    this = NULL;
                    return correct_key_code (c);
                }
                /* Did not find a match or {c} was changed in the if above,
                   so we have to return everything we had skipped
                 */
                push_char (c);
                pending_keys = seq_buffer;
                goto pend_send;
            }
        }
    }
    this = NULL;
    return correct_key_code (c);
}

/* --------------------------------------------------------------------------------------------- */
