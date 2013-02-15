/*
   Editor text keep buffer.

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru> 2013

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

/** \file
 *  \brief Source: editor text keep buffer.
 *  \author Andrew Borodin
 *  \date 2013
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "editbuffer.h"

/* --------------------------------------------------------------------------------------------- */
/*-
 *
 * here's a quick sketch of the layout: (don't run this through indent.)
 *
 * (b1 is buffers1 and b2 is buffers2)
 *
 *                                       |
 * \0\0\0\0\0m e _ f i l e . \nf i n . \n|T h i s _ i s _ s o\0\0\0\0\0\0\0\0\0
 * ______________________________________|______________________________________
 *                                       |
 * ...  |  b2[2]   |  b2[1]   |  b2[0]   |  b1[0]   |  b1[1]   |  b1[2]   | ...
 *      |->        |->        |->        |->        |->        |->        |
 *                                       |
 *           _<------------------------->|<----------------->_
 *                       curs2           |       curs1
 *           ^                           |                   ^
 *           |                          ^|^                  |
 *         cursor                       |||                cursor
 *                                      |||
 *                              file end|||file beginning
 *                                       |
 *                                       |
 *
 *           _
 * This_is_some_file
 * fin.
 *
 *
 * This is called a "gab buffer".
 * See also:
 * http://en.wikipedia.org/wiki/Gap_buffer
 * http://stackoverflow.com/questions/4199694/data-structure-for-text-editor
 */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialize editor buffers.
 *
 * @param buf pointer to editor buffer
 */

void
edit_buffer_init (edit_buffer_t * buf)
{
    memset (buf->buffers1, 0, sizeof (buf->buffers1));
    memset (buf->buffers2, 0, sizeof (buf->buffers2));
    buf->curs1 = 0;
    buf->curs2 = 0;
    buf->buffers2[0] = g_malloc0 (EDIT_BUF_SIZE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Clean editor buffers.
 *
 * @param buf pointer to editor buffer
 */

void
edit_buffer_clean (edit_buffer_t * buf)
{
    size_t j;

    for (j = 0; j <= MAXBUFF; j++)
    {
        g_free (buf->buffers1[j]);
        g_free (buf->buffers2[j]);
    }
}

/* --------------------------------------------------------------------------------------------- */
