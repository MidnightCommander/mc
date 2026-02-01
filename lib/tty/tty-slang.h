
#ifndef MC__TTY_SLANG_H
#define MC__TTY_SLANG_H

#include <slang.h>

/*** typedefs(not structures) and defined constants **********************************************/

#define KEY_F(x)       (1000 + x)

#define COLS           SLtt_Screen_Cols
#define LINES          SLtt_Screen_Rows

#define ENABLE_SHADOWS 1

/*** enums ***************************************************************************************/

enum
{
    KEY_BACKSPACE = 400,
    KEY_END,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_A1,
    KEY_C1,
    KEY_NPAGE,
    KEY_PPAGE,
    KEY_IC,
    KEY_ENTER,
    KEY_DC,
    KEY_SCANCEL,
    KEY_BTAB
};

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif
