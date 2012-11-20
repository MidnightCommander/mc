
/** \file radio.h
 *  \brief Header: WRadio widget
 */

#ifndef MC__WIDGET_RADIO_H
#define MC__WIDGET_RADIO_H

/*** typedefs(not structures) and defined constants **********************************************/

#define RADIO(x) ((WRadio *)(x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WRadio
{
    Widget widget;
    unsigned int state;         /* radio button state */
    int pos, sel;
    int count;                  /* number of members */
    hotkey_t *texts;            /* texts of labels */
} WRadio;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WRadio *radio_new (int y, int x, int count, const char **text);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_RADIO_H */
