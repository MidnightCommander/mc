
/** \file lib/widget/history.h
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

/* read history to the mc_config, but don't save config to file */
GList *history_get (const char *input_name);
/* load history form the mc_config */
GList *history_load (struct mc_config_t *cfg, const char *name);
/* save history to the mc_config, but don't save config to file */
void history_save (struct mc_config_t *cfg, const char *name, GList * h);
/* for repositioning of history dialog we should pass widget to this
 * function, as position of history dialog depends on widget's position */
char *history_show (GList ** history, Widget * widget, int current);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_HISTORY_H */
