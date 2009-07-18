
/** \file tty-internal.h
 *  \brief Header: internal suff of the terminal controlling library
 */

#ifndef MC_TTY_INTERNAL_H
#define MC_TTY_INTERNAL_H

#include "../../src/global.h"		/* include <glib.h> */

/* If true lines are shown by spaces */
extern gboolean slow_tty;

/* The mouse is currently: TRUE - enabled, FALSE - disabled */
extern gboolean mouse_enabled;

#endif			/* MC_TTY_INTERNAL_H */
