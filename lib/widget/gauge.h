
/** \file gauge.h
 *  \brief Header: WGauge widget
 */

#ifndef MC__WIDGET_GAUGE_H
#define MC__WIDGET_GAUGE_H

/*** typedefs(not structures) and defined constants **********************************************/

#define GAUGE(x) ((WGauge *) (x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WGauge
{
    Widget widget;
    gboolean shown;
    int max;
    int current;
    gboolean from_left_to_right;
    gboolean terminal_progress;  // report the progress to the terminal as well (OSC 9;4)
    int terminal_percentage;     // last percentage reported to the terminal, -1 if none
} WGauge;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WGauge *gauge_new (int y, int x, int cols, gboolean shown, int max, int current);
void gauge_set_value (WGauge *g, int max, int current);
void gauge_show (WGauge *g, gboolean shown);

/*** inline functions ****************************************************************************/

#endif
