/* Paneltext XView package - public declarations
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


#ifndef PANELTEXT_DEFINED
#define PANELTEXT_DEFINED

#include <xview/generic.h>
#include <xview/panel.h>

/*
 * type
 */
typedef Xv_opaque Paneltext;

/*
 * package
 */

extern Xv_pkg   	paneltext_pkg;
#define PANELTEXT	&paneltext_pkg

/*
 * attributes
 */
#define ATTR_PANELTEXT	ATTR_PKG_UNUSED_LAST-22
#define PANELTEXT_ATTR(type, ordinal)	ATTR(ATTR_PANELTEXT, type, ordinal)

typedef enum {

	PANELTEXT_NOTIFY	= PANELTEXT_ATTR(ATTR_FUNCTION_PTR,	1),
	PANELTEXT_CARETPOS	= PANELTEXT_ATTR(ATTR_INT,		2),

} Paneltext_attr;

#endif

