/* Routines expected by the Midnight Commander
   
   Copyright (C) 1999 The Free Software Foundation.

   Author Miguel de Icaza

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

   This file just has dummy procedures that do nothing under X11 editions.
   They are called from the VFS library, which is shared with the textmode
   edition, so it's impossible to eliminate then using macros.

   */

void
enable_interrupt_key(void)
{
}
   
void
disable_interrupt_key(void)
{
}

/* FIXME: We could provide a better way of doing this */
int
got_interrupt (void)
{
    return 0;
}
