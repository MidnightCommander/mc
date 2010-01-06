
/** \file tty-internal.h
 *  \brief Header: internal suff of the terminal controlling library
 */

#ifndef MC_TTY_INTERNAL_H
#define MC_TTY_INTERNAL_H

#include "../../src/global.h"   /* include <glib.h> */

/* If true lines are shown by spaces */
extern gboolean slow_tty;

/* If true use +, -, | for line drawing */
extern gboolean ugly_line_drawing;

/* The mouse is currently: TRUE - enabled, FALSE - disabled */
extern gboolean mouse_enabled;

char *mc_tty_normalize_from_utf8 (const char *);

#endif /* MC_TTY_INTERNAL_H */
