/** \file listbox-window.h
 *  \brief Header: Listbox widget, a listbox within dialog window
 */

#ifndef MC__LISTBOX_DIALOG_H
#define MC__LISTBOX_DIALOG_H

/*** typedefs(not structures) and defined constants **********************************************/

#define LISTBOX_APPEND_TEXT(l,h,t,d) \
    listbox_add_item (l->list, LISTBOX_APPEND_AT_END, h, t, d)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    struct WDialog *dlg;
    struct WListbox *list;
} Listbox;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Listbox utility functions */
Listbox *create_listbox_window_centered (int center_y, int center_x, int lines, int cols,
                                         const char *title, const char *help);
Listbox *create_listbox_window (int lines, int cols, const char *title, const char *help);
int run_listbox (Listbox * l);

/*** inline functions ****************************************************************************/

#endif /* MC__LISTBOX_DIALOG_H */
