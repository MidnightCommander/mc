#ifndef MC__EVENT_H
#define MC__EVENT_H

#include <config.h>

#include "../src/global.h"      /* <glib.h> */


/*** typedefs(not structures) and defined constants **********************************************/
struct mcevent_struct;


typedef gboolean (*mcevent_callback_func) (struct mcevent_struct *, gpointer);


/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mcevent_struct
{
    gchar *name;
    mcevent_callback_func cb;
    gpointer data;
} mcevent_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/


/* event.c: */
gboolean mcevent_init(void);
void mcevent_deinit(void);
void mcevent_run(void);
void mcevent_stop(void);

GList *mcevent_get_list (void);
mcevent_t *mcevent_get_handler(const gchar *);
gboolean mcevent_raise(const gchar *, gpointer);


/* callback.c: */
gboolean mcevent_add_cb(const gchar *,  mcevent_callback_func, gpointer);
gboolean mcevent_del_cb(const gchar *);


#endif
