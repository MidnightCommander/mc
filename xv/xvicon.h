/* XView Onroot Icon from Pixmap creation header file.
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

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <xview/xview.h>
#include <xview/frame.h>

typedef struct _XpmIcon {
    Pixmap pixmap;
    Pixmap mask;
    XpmAttributes attributes;
    char *filename;
    Frame frame;
} XpmIcon;

XpmIcon *CreateXpmIcon (char *iconname, int x, int y, char *title);
void DeleteXpmIcon (XpmIcon *view);
