/** \file timer.h
 *  \brief Header: simple timer
 */

#ifndef MC_TIMER_H
#define MC_TIMER_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct mc_timer_t;
typedef struct mc_timer_t mc_timer_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

mc_timer_t *mc_timer_new (void);
void mc_timer_destroy (mc_timer_t * timer);
guint64 mc_timer_elapsed (const mc_timer_t * timer);

/*** inline functions **************************************************/

#endif /* MC_TIMER_H */
