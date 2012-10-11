
/** \file check.h
 *  \brief Header: WCheck widget
 */

#ifndef MC__WIDGET_CHECK_H
#define MC__WIDGET_CHECK_H

/*** typedefs(not structures) and defined constants **********************************************/

#define CHECK(x) ((WCheck *)(x))

#define C_BOOL   0x0001
#define C_CHANGE 0x0002

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WCheck
{
    Widget widget;
    unsigned int state;         /* check button state */
    hotkey_t text;              /* text of check button */
} WCheck;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WCheck *check_new (int y, int x, int state, const char *text);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_CHECK_H */
