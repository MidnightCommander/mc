
/** \file color-internal.h
 *  \brief Header: Internal stuff of color setup
 */

#ifndef MC__COLOR_INTERNAL_H
#define MC__COLOR_INTERNAL_H

#include <sys/types.h>  // size_t

#include "lib/global.h"

#ifdef HAVE_SLANG
#include "tty-slang.h"
#else
#include "tty-ncurses.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

#define FLAG_TRUECOLOR   (1 << 24)
#define COLORS_TRUECOLOR (1 << 24)

/*** enums ***************************************************************************************/

typedef enum
{
    SPEC_A_REVERSE = -100,
    SPEC_A_BOLD = -101,
    SPEC_A_BOLD_REVERSE = -102,
    SPEC_A_UNDERLINE = -103
} tty_special_color_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* Screen library specific color pair */
typedef struct
{
    int fg;
    int bg;
    int attr;
    size_t pair_index;
    gboolean is_temp;
} tty_color_lib_pair_t;

/*** global variables defined in .c file *********************************************************/

extern gboolean use_colors;
extern gboolean mc_tty_color_disable;

/*** declarations of public functions ************************************************************/

const char *tty_color_get_name_by_index (int idx);
int tty_color_get_index_by_name (const char *color_name);
int tty_attr_get_bits (const char *attrs);

void tty_color_init_lib (gboolean disable, gboolean force);
void tty_color_deinit_lib (void);

void tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair);

/*** inline functions ****************************************************************************/

#endif
