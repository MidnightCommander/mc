
/** \file label.h
 *  \brief Header: WLabel widget
 */

#ifndef MC__WIDGET_LABEL_H
#define MC__WIDGET_LABEL_H

/*** enums ***************************************************************************************/

typedef enum
{
    LABEL_COLOR_MAIN,
    LABEL_COLOR_DISABLED,
    LABEL_COLOR_COUNT
} label_colors_enum_t;

/*** typedefs(not structures) and defined constants **********************************************/

#define LABEL(x) ((WLabel *) (x))

typedef int label_colors_t[LABEL_COLOR_COUNT];

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    Widget widget;
    gboolean auto_adjust_cols;  // compute widget.cols from strlen(text)?
    char *text;
    const int *color;  // NULL to inherit from parent widget
} WLabel;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WLabel *label_new (int y, int x, const char *text);
void label_set_text (WLabel *label, const char *text);
void label_set_textv (WLabel *label, const char *format, ...) G_GNUC_PRINTF (2, 3);

/*** inline functions ****************************************************************************/

#endif
