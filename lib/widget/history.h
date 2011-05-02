
/** \file history.h
 *  \brief Header: save, load and show history
 */

#ifndef MC__WIDGET_HISTORY_H
#define MC__WIDGET_HISTORY_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* forward declaration */
struct mc_config_t;

/*** global variables defined in .c file *********************************************************/

extern int num_history_items_recorded;

/*** declarations of public functions ************************************************************/

GList *history_get (const char *input_name);
/* save history to the mc_config, but don't save config to file */
void history_save (struct mc_config_t * cfg, const char *name, GList * h);
#if 0
/* write history to the ${XDG_CACHE_HOME}/mc/history file */
void history_put (const char *input_name, GList * h);
#endif
/* for repositioning of history dialog we should pass widget to this
 * function, as position of history dialog depends on widget's position */
char *history_show (GList ** history, Widget * widget);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_HISTORY_H */
