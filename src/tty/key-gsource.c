/* Keyboard support routines.
   Handle keyboard via GMainLoop.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995 Miguel de Icaza.
	       1994, 1995 Janne Kukonlehto.
	       1995  Jakub Jelinek.
	       1997  Norbert Warmuth

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

/** \file key.c
 *  \brief Source: keyboard support routines
 */

#include <config.h>


#include "../../src/global.h"
#include "../../src/tty/key-internal.h"
#include "../../src/event/event.h"

/*** global variables **************************************************/

tty_key_event_t tty_key_event;

/*** file scope macro definitions **************************************/

/* The maximum sequence length (32 + null terminator) */
#define SEQ_BUFFER_LEN 33

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

static GIOChannel *tty_key_channel_key;

/*** file scope functions **********************************************/

/* --------------------------------------------------------------------------------------------- */
static gboolean
tty_key_gsource_keyboard_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
    tty_key_event_t *tty_key_event;
    int seq_buffer [SEQ_BUFFER_LEN];
    int seq_count=0;

    gchar buf;
    gsize bytes_read;

    while (g_io_channel_read_chars (source, &buf, 1 , &bytes_read, NULL) == G_IO_STATUS_NORMAL)
    {
	seq_buffer [seq_count++] = (int) buf;
    }
    /*
    TODO:
    1) parse seq_buffer
    2) handle ESC, <key> begavior
    3) handle mouse
    */

    tty_key_event.key = 0;
    mcevent_raise("keyboard.press", (gpointer) &tty_key_event);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
static void
tty_key_gsource_keyboard_init(void)
{
#ifdef __CYGWIN__
    tty_key_channel_key = g_io_channel_win32_new (input_fd);
#else
    tty_key_channel_key = g_io_channel_unix_new (input_fd);
#endif

    g_io_channel_set_encoding (tty_key_channel_key, NULL, NULL);
    g_io_channel_set_buffered (tty_key_channel_key, FALSE);
    g_io_channel_set_flags (tty_key_channel_key, G_IO_FLAG_NONBLOCK, NULL);

    g_io_add_watch (tty_key_channel_key, G_IO_IN /*|G_IO_HUP|G_IO_PRI|G_IO_ERR|G_IO_NVAL*/ , tty_key_gsource_keyboard_callback, NULL);

}
/* --------------------------------------------------------------------------------------------- */

/*** public functions **************************************************/

/* --------------------------------------------------------------------------------------------- */

void
tty_key_gsource_init(void)
{
    tty_key_gsource_keyboard_init();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_key_gsource_deinit(void)
{

}

/* --------------------------------------------------------------------------------------------- */
