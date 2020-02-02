#ifndef MC_GLIBCOMPAT_H
#define MC_GLIBCOMPAT_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

#if ! GLIB_CHECK_VERSION (2, 63, 3)
void g_clear_slist (GSList ** slist_ptr, GDestroyNotify destroy);
void g_clear_list (GList ** list_ptr, GDestroyNotify destroy);
#endif /* ! GLIB_CHECK_VERSION (2, 63, 3) */

#if ! GLIB_CHECK_VERSION (2, 32, 0)
void g_queue_free_full (GQueue * queue, GDestroyNotify free_func);
#endif /* ! GLIB_CHECK_VERSION (2, 32, 0) */

#if ! GLIB_CHECK_VERSION (2, 60, 0)
void g_queue_clear_full (GQueue * queue, GDestroyNotify free_func);
#endif /* ! GLIB_CHECK_VERSION (2, 60, 0) */

/*** inline functions ****************************************************************************/

#endif /* MC_GLIBCOMPAT_H */
