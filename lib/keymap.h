#ifndef MC__KEYMAP_H
#define MC__KEYMAP_H

#include "lib/event.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct
{
    const char *name;
    const char *event_group;
    const char *event_name;
} mc_keymap_event_init_group_t;

typedef struct
{
    const char *group;
    const mc_keymap_event_init_group_t *keymap_events;
} mc_keymap_event_init_t;


/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean mc_keymap_init (GError ** error);
gboolean mc_keymap_deinit (GError ** error);

gboolean mc_keymap_bind_keycode (const char *group, const char *name, const char *pressed_keynames,
                                 gboolean isDeleteOld, GError ** error);

gboolean mc_keymap_bind_event (const char *group, const char *name, const char *event_group,
                               const char *event_name, GError ** error);
gboolean mc_keymap_mass_bind_event (const mc_keymap_event_init_t *, GError ** error);

gboolean mc_keymap_bind_switch_event (const char *group, const char *name, const char *switch_group,
                                      GError ** error);

gboolean mc_keymap_process_group (const char *group, long pressed_keycode, void *data,
                                  event_return_t * ret, GError ** error);
const char *mc_keymap_get_key_name_by_code (const char *group, long pressed_keycode,
                                            GError ** error);


/*** inline functions ****************************************************************************/

#endif /* MC__KEYMAP_H */
