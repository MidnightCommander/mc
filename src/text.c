/* Text edition support code.

   Copyright (C) 2001-2002 Free Software Foundation

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <stdio.h>

#define WANT_WIDGETS
#include "global.h"
#include "win.h"
#include "tty.h"
#include "key.h"
#include "widget.h"
#include "main.h"
#include "cons.saver.h"
#include "textconf.h"

char *default_edition_colors =
"normal=lightgray,blue:"
"selected=black,cyan:"
"marked=yellow,blue:"
"markselect=yellow,cyan:"
"errors=white,red:"
"menu=white,cyan:"
"reverse=black,lightgray:"
"dnormal=black,lightgray:"
"dfocus=black,cyan:"
"dhotnormal=blue,lightgray:"
"dhotfocus=blue,cyan:"
"viewunderline=brightred,blue:"
"menuhot=yellow,cyan:"
"menusel=white,black:"
"menuhotsel=yellow,black:"
"helpnormal=black,lightgray:"
"helpitalic=red,lightgray:"
"helpbold=blue,lightgray:"
"helplink=black,cyan:"
"helpslink=yellow,blue:"
"gauge=white,black:"
"input=black,cyan:"
"directory=white,blue:"
"executable=brightgreen,blue:"
"link=lightgray,blue:"
"stalelink=brightred,blue:"
"device=brightmagenta,blue:"
"core=red,blue:"
"special=black,blue:"
"editnormal=lightgray,blue:"
"editbold=yellow,blue:"
"editmarked=black,cyan";

void
clr_scr (void)
{
    standend ();
    dlg_erase (midnight_dlg);
    mc_refresh ();
    doupdate ();
}

