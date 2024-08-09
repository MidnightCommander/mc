/*
   X11 support for the Midnight Commander.

   Copyright (C) 2005-2024
   Free Software Foundation, Inc.

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file x11conn.c
 *  \brief Source: X11 support
 *  \warning This code uses setjmp() and longjmp(). Before you modify _anything_ here,
 *  please read the relevant sections of the C standard.
 */

#include <config.h>

#include <setjmp.h>
#include <X11/Xlib.h>
#ifdef HAVE_GMODULE
#include <gmodule.h>
#endif

#include "lib/global.h"
#include "x11conn.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifndef HAVE_GMODULE
#define func_XOpenDisplay       XOpenDisplay
#define func_XCloseDisplay      XCloseDisplay
#define func_XSetErrorHandler   XSetErrorHandler
#define func_XSetIOErrorHandler XSetIOErrorHandler
#define func_XQueryPointer      XQueryPointer
#endif

/*** file scope type declarations ****************************************************************/

typedef int (*mc_XErrorHandler_callback) (Display *, XErrorEvent *);
typedef int (*mc_XIOErrorHandler_callback) (Display *);

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

#ifdef HAVE_GMODULE
static Display *(*func_XOpenDisplay) (_Xconst char *);
static int (*func_XCloseDisplay) (Display *);
static mc_XErrorHandler_callback (*func_XSetErrorHandler) (mc_XErrorHandler_callback);
static mc_XIOErrorHandler_callback (*func_XSetIOErrorHandler) (mc_XIOErrorHandler_callback);
static Bool (*func_XQueryPointer) (Display *, Window, Window *, Window *,
                                   int *, int *, int *, int *, unsigned int *);

static GModule *x11_module;
#endif

static gboolean handlers_installed = FALSE;

/* This flag is set as soon as an X11 error is reported. Usually that
 * means that the DISPLAY is not available anymore. We do not try to
 * reconnect, as that would violate the X11 protocol. */
static gboolean lost_connection = FALSE;

static jmp_buf x11_exception;   /* FIXME: get a better name */
static gboolean longjmp_allowed = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
x_io_error_handler (Display *dpy)
{
    (void) dpy;

    lost_connection = TRUE;
    if (longjmp_allowed)
    {
        longjmp_allowed = FALSE;
        longjmp (x11_exception, 1);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
x_error_handler (Display *dpy, XErrorEvent *ee)
{
    (void) ee;
    (void) func_XCloseDisplay (dpy);
    return x_io_error_handler (dpy);
}

/* --------------------------------------------------------------------------------------------- */

static void
install_error_handlers (void)
{
    if (handlers_installed)
        return;

    (void) func_XSetErrorHandler (x_error_handler);
    (void) func_XSetIOErrorHandler (x_io_error_handler);
    handlers_installed = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
x11_available (void)
{
#ifdef HAVE_GMODULE
    if (lost_connection)
        return FALSE;

    if (x11_module != NULL)
        return TRUE;

    x11_module = g_module_open ("X11", G_MODULE_BIND_LAZY);
    if (x11_module == NULL)
        x11_module = g_module_open ("libX11.so.6", G_MODULE_BIND_LAZY);

    if (x11_module == NULL)
        return FALSE;

    if (!g_module_symbol (x11_module, "XOpenDisplay", (void *) &func_XOpenDisplay))
        goto cleanup;
    if (!g_module_symbol (x11_module, "XCloseDisplay", (void *) &func_XCloseDisplay))
        goto cleanup;
    if (!g_module_symbol (x11_module, "XQueryPointer", (void *) &func_XQueryPointer))
        goto cleanup;
    if (!g_module_symbol (x11_module, "XSetErrorHandler", (void *) &func_XSetErrorHandler))
        goto cleanup;
    if (!g_module_symbol (x11_module, "XSetIOErrorHandler", (void *) &func_XSetIOErrorHandler))
        goto cleanup;

    install_error_handlers ();
    return TRUE;

  cleanup:
    func_XOpenDisplay = 0;
    func_XCloseDisplay = 0;
    func_XQueryPointer = 0;
    func_XSetErrorHandler = 0;
    func_XSetIOErrorHandler = 0;
    g_module_close (x11_module);
    x11_module = NULL;
    return FALSE;
#else
    install_error_handlers ();
    return !(lost_connection);
#endif
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

Display *
mc_XOpenDisplay (const char *displayname)
{
    if (x11_available ())
    {
        if (setjmp (x11_exception) == 0)
        {
            Display *retval;

            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = TRUE;

            retval = func_XOpenDisplay (displayname);

            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = FALSE;
            return retval;
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_XCloseDisplay (Display *display)
{
    if (x11_available ())
    {
        if (setjmp (x11_exception) == 0)
        {
            int retval;

            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = TRUE;

            retval = func_XCloseDisplay (display);

            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = FALSE;

            return retval;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

Bool
mc_XQueryPointer (Display *display, Window win, Window *root_return,
                  Window *child_return, int *root_x_return, int *root_y_return,
                  int *win_x_return, int *win_y_return, unsigned int *mask_return)
{
    Bool retval;

    if (x11_available ())
    {
        if (setjmp (x11_exception) == 0)
        {
            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = TRUE;

            retval = func_XQueryPointer (display, win, root_return,
                                         child_return, root_x_return, root_y_return,
                                         win_x_return, win_y_return, mask_return);

            /* cppcheck-suppress redundantAssignment */
            longjmp_allowed = FALSE;

            return retval;
        }
    }
    *root_return = None;
    *child_return = None;
    *root_x_return = 0;
    *root_y_return = 0;
    *win_x_return = 0;
    *win_y_return = 0;
    *mask_return = 0;
    return False;
}

/* --------------------------------------------------------------------------------------------- */
