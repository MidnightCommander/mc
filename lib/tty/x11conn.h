/** \file x11conn.h
 *  \brief Header: X11 support
 *  \warning This code uses setjmp() and longjmp(). Before you modify _anything_ here,
 *  please read the relevant sections of the C standard.
 */

#ifndef MC__X11CONN_H
#define MC__X11CONN_H

/*
   This module provides support for some X11 functions.  The functions
   are loaded dynamically if GModule is available, and statically if
   not.  X11 session handling is somewhat robust.  If there is an X11
   error or a connection error, all further traffic to the X server
   will be suppressed, and the functions will return reasonable default
   values.
 */

#include <X11/Xlib.h>

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

extern Display *mc_XOpenDisplay (const char *displayname);
extern int mc_XCloseDisplay (Display * display);

extern Bool mc_XQueryPointer (Display * display, Window win, Window * root_return,
                              Window * child_return, int *root_x_return, int *root_y_return,
                              int *win_x_return, int *win_y_return, unsigned int *mask_return);

/*** inline functions ****************************************************************************/

#endif /* MC__X11CONN_H */
