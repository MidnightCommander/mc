/* Midnight Commander Tk initialization.
   Copyright (C) 1995 Miguel de Icaza
   Copyright (C) 1995 Jakub Jelinek
   
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
#include "tkmain.h"
#include "tty.h"	/* If you wonder why, read the comment below */
#include "key.h"

/* The tty.h file is included since we try to use as much object code
 * from the curses distribution as possible so we need to send back
 * the same constants that the code expects on the non-X version
 * of the code.  The constants may be the ones from curses/ncurses
 * or our macro definitions for slang
 */
 
/* Describes the key code and the keysym name returned by X 
 * In case we found many keyboard problems with the Fnn keys
 * we can allways remove them and put them on the mc.tcl file
 * as local bindings
 */
static key_code_name_t x_keys [] = {
    { KEY_LEFT,      "Left" },
    { KEY_RIGHT,     "Right" },
    { KEY_UP,        "Up" },
    { KEY_DOWN,      "Down" },
    { KEY_END,       "End" },
    { KEY_END,       "R13" },
    { KEY_HOME,      "Home" },
    { KEY_HOME,      "F27" },
    { KEY_PPAGE,     "F29" },
    { KEY_PPAGE,     "Prior" },
    { KEY_NPAGE,     "Next" },
    { KEY_NPAGE,     "F35" },
    { '\n',          "Return" },
    { '\n',          "KP_Enter" },
#if 0
    { KEY_DC,        "Delete" },
#else
    { KEY_BACKSPACE, "Delete" },
#endif
    { KEY_IC,        "Insert" },
    { KEY_BACKSPACE, "BackSpace" },
    { KEY_F(1),      "F1" },
    { KEY_F(2),      "F2" },
    { KEY_F(3),      "F3" },
    { KEY_F(4),      "F4" },
    { KEY_F(5),      "F5" },
    { KEY_F(6),      "F6" },
    { KEY_F(7),      "F7" },
    { KEY_F(8),      "F8" },
    { KEY_F(9),      "F9" },
    { KEY_F(10),     "F10" },
    { 0, 0}			/* terminator */
};

int lookup_keysym (char *s)
{
    int i;
    
    for (i = 0; x_keys [i].name; i++){
	if (!strcmp (s, x_keys [i].name))
	    return (x_keys [i].code);
    }
    return 0;
}


#if TCL_MAJOR_VERSION > 7
/* Tcl 8.xx support */

void add_select_channel (int fd, select_fn callback, void *info)
{
    Tcl_CreateFileHandler (fd, TCL_READABLE, (Tcl_FileProc *) callback, 0);
}

void delete_select_channel (int fd)
{
    Tcl_DeleteFileHandler (fd);
}

#else
/* Tcl 7.xx support */
   
void add_select_channel (int fd, select_fn callback, void *info)
{
    Tcl_File handle;

    handle = Tcl_GetFile ((ClientData) fd, TCL_UNIX_FD);
    Tcl_CreateFileHandler (handle, TCL_READABLE, (Tcl_FileProc *) callback, 0);
}

void delete_select_channel (int fd)
{
    Tcl_File handle;

    handle = Tcl_GetFile ((ClientData) fd, TCL_UNIX_FD);
    Tcl_DeleteFileHandler (handle);
    Tcl_FreeFile (handle);
}
#endif

int
mi_getch ()
{
	return tk_getch();
}
