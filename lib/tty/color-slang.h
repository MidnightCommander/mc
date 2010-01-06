
/** \file color-slang.h
 *  \brief Header: S-Lang-specific color setup
 */

#ifndef MC_COLOR_SLANG_H
#define MC_COLOR_SLANG_H

#include "../../src/tty/tty-slang.h"    /* S-Lang headers */

enum {
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE
};

/* When using Slang with color, we have all the indexes free but
 * those defined here (A_BOLD, A_UNDERLINE, A_REVERSE, A_BOLD_REVERSE)
 */

#ifndef A_BOLD
#define A_BOLD SLTT_BOLD_MASK
#endif /* A_BOLD */

#endif /* MC_COLOR_SLANG_H */
