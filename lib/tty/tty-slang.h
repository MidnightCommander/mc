
#ifndef MC__TTY_SLANG_H
#define MC__TTY_SLANG_H

#ifdef HAVE_SLANG_SLANG_H
#include <slang/slang.h>
#else
#include <slang.h>
#endif /* HAVE_SLANG_SLANG_H */

/*** typedefs(not structures) and defined constants **********************************************/

#define KEY_F(x) (1000 + x)

#define ACS_VLINE    SLSMG_VLINE_CHAR
#define ACS_HLINE    SLSMG_HLINE_CHAR
#define ACS_LTEE     SLSMG_LTEE_CHAR
#define ACS_RTEE     SLSMG_RTEE_CHAR
#define ACS_TTEE     SLSMG_UTEE_CHAR
#define ACS_BTEE     SLSMG_DTEE_CHAR
#define ACS_ULCORNER SLSMG_ULCORN_CHAR
#define ACS_LLCORNER SLSMG_LLCORN_CHAR
#define ACS_URCORNER SLSMG_URCORN_CHAR
#define ACS_LRCORNER SLSMG_LRCORN_CHAR
#define ACS_PLUS     SLSMG_PLUS_CHAR

#define COLS  SLtt_Screen_Cols
#define LINES SLtt_Screen_Rows

/*** enums ***************************************************************************************/

enum
{
    KEY_BACKSPACE = 400,
    KEY_END, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_A1, KEY_C1, KEY_NPAGE, KEY_PPAGE, KEY_IC,
    KEY_ENTER, KEY_DC, KEY_SCANCEL, KEY_BTAB
};

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern int reset_hp_softkeys;

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC_TTY_SLANG_H */
