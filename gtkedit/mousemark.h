/* Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

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

#ifndef _MOUSE_MARK
#define _MOUSE_MARK

/* mouse_mark.h */

struct mouse_funcs {
    void *data;
    void (*xy) (int, int, int *, int *);
					/* (row, pixel-x-pos) from click pos (x,y) */
    long (*cp) (void *, int, int);	/* get buffer offset from (row, pixel-x-pos) */
    int (*marks) (void *, long *, long *);
					/* get mark start and end */
    int (*range) (void *, long, long, long);
					/* is click between marks? */
    void (*fin_mark) (void *);		/* set marks to not-moving mode (optional) */
    void (*move_mark) (void *);		/* set marks to moving mode (optional) */
    void (*release_mark) (void *, XEvent *);
					/* set marks when button is release.
					   possibly sets selection owner (could be
					   same as fin_mark) (optional) */
    char * (*get_block) (void *, long, long, int *, int *);
					/* return block from between marks,
					   result is free'd by mouse_mark
					   int *, int * is Dnd type, length */
    void (*move) (void *, long, int);	/* move cursor, int is row if needed */
    void (*motion) (void *, long);	/* move second mark position */
    void (*dclick) (void *, XEvent *);	/* called on double click (optional) */
    void (*redraw) (void *, long);/* called before exit, passing mouse
					   pos in buffer (optional) */
    int (*insert_drop) (void *, Window, unsigned char *, int, int, int, Atom, Atom);
    void (*delete_block) (void *);

/* this is one of the old dnd types. these are our preference
for drag between cooledits */
    int types;

/* anything else that the widget can recieve eg {"text", "video"} */
    char **mime_majors;
};

void mouse_mark (XEvent * event, int double_click, struct mouse_funcs *funcs);

struct mouse_funcs *mouse_funcs_new (void *data, struct mouse_funcs *m);
void mouse_shut (void);
void mouse_init (void);

extern Atom **xdnd_typelist_receive;
extern Atom **xdnd_typelist_send;

#endif

