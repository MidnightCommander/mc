
/** \file color-internal.h
 *  \brief Header: Internal stuff of color setup
 */

#ifndef MC_COLOR_INTERNAL_H
#define MC_COLOR_INTERNAL_H

#include <sys/types.h>			/* size_t */

#include "../../src/global.h"

#ifdef HAVE_SLANG
#   include "../../src/tty/tty-slang.h"
#else
#   include "../../src/tty/tty-ncurses.h"
#endif			/* HAVE_SLANG */

extern gboolean use_colors;
extern gboolean mc_tty_color_disable;


#ifdef HAVE_SLANG
#   define CTYPE const char *
#else
#   define CTYPE int
#endif			/* HAVE_SLANG */

typedef struct mc_color_pair_struct {
    const char *cfg;
    const char *cbg;
    int ifg;
    int ibg;
    int pair_index;
} mc_color_pair_t;

typedef enum
{
	SPEC_A_REVERSE		= -100,
	SPEC_A_BOLD		= -101,
	SPEC_A_BOLD_REVERSE	= -102,
	SPEC_A_UNDERLINE	= -103
} mc_tty_color_special_t;

const char *mc_tty_color_get_valid_name(const char *);
int mc_tty_color_get_index_by_name(const char *);

void mc_tty_color_init_lib (gboolean, gboolean);
void mc_tty_color_try_alloc_pair_lib (mc_color_pair_t *);
void mc_tty_color_set_lib (int);
void mc_tty_color_set_normal_attrs_lib (void);
void mc_tty_color_lowlevel_set_lib (int);

#endif				/* MC_COLOR_INTERNAL_H */
