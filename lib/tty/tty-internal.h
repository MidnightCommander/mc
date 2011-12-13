
/** \file tty-internal.h
 *  \brief Header: internal suff of the terminal controlling library
 */

#ifndef MC__TTY_INTERNAL_H
#define MC__TTY_INTERNAL_H

#include "lib/global.h"         /* include <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/* Taken from S-Lang's slutty.c */
#ifdef ultrix                   /* Ultrix gets _POSIX_VDISABLE wrong! */
#define NULL_VALUE -1
#else
#ifdef _POSIX_VDISABLE
#define NULL_VALUE _POSIX_VDISABLE
#else
#define NULL_VALUE 255
#endif
#endif

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/* The mouse is currently: TRUE - enabled, FALSE - disabled */
extern gboolean mouse_enabled;

/* terminal ca capabilities */
extern char *smcup;
extern char *rmcup;

/*** declarations of public functions ************************************************************/

char *mc_tty_normalize_from_utf8 (const char *);
void tty_init_xterm_support (gboolean is_xterm);
int tty_lowlevel_getch (void);

/*** inline functions ****************************************************************************/
#endif /* MC_TTY_INTERNAL_H */
