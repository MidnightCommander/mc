
/** \file buttonbar.h
 *  \brief Header: WButtonBar widget
 */

#ifndef MC__WIDGET_BUTTONBAR_H
#define MC__WIDGET_BUTTONBAR_H

/*** typedefs(not structures) and defined constants **********************************************/

#define BUTTONBAR(x) ((WButtonBar *)(x))

/* number of bttons in buttonbar */
#define BUTTONBAR_LABELS_NUM 10

#define buttonbar_clear_label(bb, idx, recv) buttonbar_set_label (bb, idx, "", NULL, recv)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct global_keymap_t;

typedef struct WButtonBar
{
    Widget widget;
    gboolean visible;           /* Is it visible? */
    struct
    {
        char *text;
        unsigned long command;
        Widget *receiver;
        int end_coord;          /* cumulative width of buttons so far */
    } labels[BUTTONBAR_LABELS_NUM];
} WButtonBar;

struct global_keymap_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WButtonBar *buttonbar_new (gboolean visible);
void buttonbar_set_label (WButtonBar * bb, int idx, const char *text,
                          const struct global_keymap_t *keymap, const Widget * receiver);
WButtonBar *find_buttonbar (const WDialog * h);

/*** inline functions ****************************************************************************/

static inline void
buttonbar_set_visible (WButtonBar * bb, gboolean visible)
{
    bb->visible = visible;
}

#endif /* MC__WIDGET_BUTTONBAR_H */
