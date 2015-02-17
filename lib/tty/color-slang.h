
/** \file color-slang.h
 *  \brief Header: S-Lang-specific color setup
 */

#ifndef MC__COLOR_SLANG_H
#define MC__COLOR_SLANG_H

#include "tty-slang.h"          /* S-Lang headers */

/*** typedefs(not structures) and defined constants **********************************************/

/* When using Slang with color, we have all the indexes free but
 * those defined here (A_BOLD, A_ITALIC, A_UNDERLINE, A_REVERSE, A_BLINK)
 */

#ifndef A_BOLD
#define A_BOLD SLTT_BOLD_MASK
#endif /* A_BOLD */
#ifdef SLTT_ITALIC_MASK         /* available since slang-pre2.3.0-107 */
#ifndef A_ITALIC
#define A_ITALIC SLTT_ITALIC_MASK
#endif /* A_ITALIC */
#endif /* SLTT_ITALIC_MASK */
#ifndef A_UNDERLINE
#define A_UNDERLINE SLTT_ULINE_MASK
#endif /* A_UNDERLINE */
#ifndef A_REVERSE
#define A_REVERSE SLTT_REV_MASK
#endif /* A_REVERSE */
#ifndef A_BLINK
#define A_BLINK SLTT_BLINK_MASK
#endif /* A_BLINK */

/*** enums ***************************************************************************************/

enum
{
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE
};

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/
#endif /* MC_COLOR_SLANG_H */
