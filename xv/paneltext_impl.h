/* Paneltext XView package - private declarations.
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

#ifndef PANELTEXT_IMPL_DEFINED
#define PANELTEXT_IMPL_DEFINED

typedef struct {
	void (*notify)();
	void (*accept_key)();
} Paneltext_info;

typedef struct {
        Xv_panel_text   parent_data;
        Xv_opaque       private_data;
} Paneltext_struct;

#define PANELTEXT_PRIVATE(paneltext)	\
		XV_PRIVATE(Paneltext_info, Paneltext_struct, paneltext)

#endif

