
/** \file color-internal.h
 *  \brief Header: Internal stuff of color setup
 */

#ifndef MC_COLOR_INTERNAL_H
#define MC_COLOR_INTERNAL_H

#include <sys/types.h>          /* size_t */

#include "lib/global.h"

#ifdef HAVE_SLANG
#   include "tty-slang.h"
#else
#   include "tty-ncurses.h"
#endif /* HAVE_SLANG */

extern gboolean use_colors;
extern gboolean mc_tty_color_disable;

typedef struct mc_color_pair_struct
{
    const char *cfg;
    const char *cbg;
    int ifg;
    int ibg;
    size_t pair_index;
    gboolean is_temp;
} tty_color_pair_t;

/* *INDENT-OFF* */
typedef enum {
    SPEC_A_REVERSE		= -100,
    SPEC_A_BOLD			= -101,
    SPEC_A_BOLD_REVERSE		= -102,
    SPEC_A_UNDERLINE		= -103
} tty_special_color_t;
/* *INDENT-ON* */

const char *tty_color_get_valid_name (const char *);
int tty_color_get_index_by_name (const char *);

void tty_color_init_lib (gboolean, gboolean);
void tty_color_deinit_lib (void);

void tty_color_try_alloc_pair_lib (tty_color_pair_t *);

#endif /* MC_COLOR_INTERNAL_H */
