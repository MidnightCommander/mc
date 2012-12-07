/*
   GLIB - Library of useful routines for C programming

   Copyright (C) 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

/** \file glibcompat.c
 *  \brief Source: compatibility with older versions of glib
 *
 *  Following code was copied from glib to GNU Midnight Commander to
 *  provide compatibility with older versions of glib.
 */

#include <config.h>

#include "global.h"
#include "glibcompat.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#if ! GLIB_CHECK_VERSION (2, 13, 0)
/*
   This is incomplete copy of same glib-function.
   For older glib (less than 2.13) functional is enought.
   For full version of glib welcome to glib update.
 */
gboolean
g_unichar_iszerowidth (gunichar c)
{
    if (G_UNLIKELY (c == 0x00AD))
        return FALSE;

    if (G_UNLIKELY ((c >= 0x1160 && c < 0x1200) || c == 0x200B))
        return TRUE;

    return FALSE;
}
#endif /* ! GLIB_CHECK_VERSION (2, 13, 0) */

/* --------------------------------------------------------------------------------------------- */
