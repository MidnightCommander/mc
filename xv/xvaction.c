/* XView Action Icons (for Drag and Drop).
   Copyright (C) 1995 Jakub Jelinek.
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>

#include <stdlib.h>
#include "setup.h"
#include "profile.h"
#include "xvmain.h"
#include "ext.h"
#include "mad.h"

#ifdef HAVE_XPM_SHAPE
#include "xvicon.h"

extern char *regex_command_title;

extern Display *dpy;

void add_action_icon (char *filename, char *geometry)
{
    char *iconname, *title, *p, *base = x_basename (filename);
    int x, y, z;
    XpmIcon *icon;

    iconname = regex_command (base, "Icon", NULL, NULL);
    if (iconname == NULL)
        iconname = strdup ("file.xpm");
    if (*iconname != '/') {
        p = copy_strings (ICONDIR, iconname, NULL);
        free (iconname);
        iconname = p;
    }
    title = regex_command_title;
    if (title == NULL)
        title = strdup (base);
    else {
        char *q, *r;
        
        y = strlen (filename);
        z = strlen (base);
        for (q = title, x = 1; *q; q++, x++)
            if (*q == '%') {
                if (q [1] == 'p')
		    x += z - 2;
		else if (q [1] == 'd')
		    x += y - 2;
	    }
	r = xmalloc (x, "Icon Title");
	for (q = title, p = r; *q; q++, p++)
            if (*q == '%') {
                if (q [1] == 'p') {
		    strcpy (p, base);
		    p += z - 1;
		    q++;
		} else if (q [1] == 'd') {
		    strcpy (p, filename);
		    p += y - 1;
		    q++;
		} else
		    *p = *q;
	    } else
	        *p = *q;
	*p = 0;
	free (title);
	title = r;
    }
    x = atoi (geometry);
    for (p = geometry; *p && (*p < '0' || *p > '9'); p++);
    for (; *p >= '0' && *p <= '9'; p++);
    y = atoi (p);
    icon = CreateXpmIcon (iconname, x, y, title);
    if (icon != NULL) {
        icon->filename = strdup (filename);
    }
    free (iconname);
    free (title);
    XFlush (dpy);
    xv_dispatch_a_bit ();
}
#endif

void xv_action_icons (void)
{
#ifdef HAVE_XPM_SHAPE
    char *key, *value;
    void *keys = profile_init_iterator ("Action Icons", profile_name);

    xv_dispatch_a_bit ();
    if (keys == NULL) {
        add_action_icon ("/bin/rm", "+45+100");
        add_action_icon ("/usr/bin/lpr", "+45+160");
    }
    
    while (keys != NULL) {
        keys = profile_iterator_next (keys, &key, &value);
        add_action_icon (key, value);
    }
#endif
}
