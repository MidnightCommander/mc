/* lkeysym.h - for any undefined keys
   Copyright (C) 1996, 1997 Paul Sheer

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */

#include <X11/keysym.h>

#ifndef XK_Page_Up
#define XK_Page_Up XK_Prior
#endif

#ifndef XK_Page_Down
#define XK_Page_Down XK_Next
#endif

#ifndef XK_KP_Page_Up
#define XK_KP_Page_Up XK_KP_Prior
#endif

#ifndef XK_KP_Page_Down
#define XK_KP_Page_Down XK_KP_Next
#endif

#ifndef XK_ISO_Left_Tab
#define	XK_ISO_Left_Tab					0xFE20
#endif
