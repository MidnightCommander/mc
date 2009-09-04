/*
   Skins engine.
   Work with colors - backward compability

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <sys/types.h>				/* size_t */
#include "../src/tty/color.h"

#include "../src/global.h"
#include "../src/setup.h"


/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
configure_colors_string (const char *the_color_string)
{
/*
    const size_t map_len = color_map_len ();

    size_t i;
    char **color_strings, **p;

    if (!the_color_string)
	return;

    color_strings = g_strsplit (the_color_string, ":", -1);

    p = color_strings;

    while ((p != NULL) && (*p != NULL)) {
	char **cfb;
	
	// color, fore, back
	// cfb[0] - entry name
	// cfb[1] - fore color
	// cfb[2] - back color

	char *e;

	cfb = g_strsplit_set (*p, "=,", 3);
	p++;

	if (cfb[0] == NULL) {
	    g_strfreev (cfb);
	    continue;
	}

	// append '=' to the entry name 
	e = g_strdup_printf ("%s=", cfb[0]);
	g_free (cfb[0]);
	cfb[0] = e;

	for (i = 0; i < map_len; i++)
	    if (color_map [i].name != NULL) {
		size_t klen = strlen (color_map [i].name);

		if (strncmp (cfb[0], color_map [i].name, klen) == 0) {
		    if ((cfb[1] != NULL) && (*cfb[1] != '\0'))
			get_color (cfb[1], &color_map [i].fg);
		    if ((cfb[2] != NULL) && (*cfb[2] != '\0'))
			get_color (cfb[2], &color_map [i].bg);
		    break;
		}
	    }

	g_strfreev (cfb);
    }

   g_strfreev (color_strings);
*/
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_colors_old_configure (void)
{
    configure_colors_string (setup_color_string);
    configure_colors_string (term_color_string);
    configure_colors_string (getenv ("MC_COLOR_TABLE"));
    configure_colors_string (command_line_colors);
}

/* --------------------------------------------------------------------------------------------- */

