#ifndef MC_GLIBCOMPAT_H
#define MC_GLIBCOMPAT_H

/*** typedefs(not structures) and defined constants **********************************************/

#ifndef G_OPTION_ENTRY_NULL
#define G_OPTION_ENTRY_NULL \
  { NULL, '\0', 0, 0, NULL, NULL, NULL }
#endif /* G_OPTION_ENTRY_NULL */

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

#if ! GLIB_CHECK_VERSION (2, 54, 0)
gboolean g_ptr_array_find_with_equal_func (GPtrArray * haystack, gconstpointer needle,
                                           GEqualFunc equal_func, guint * index_);
#endif /* ! GLIB_CHECK_VERSION (2, 54, 0) */

#if ! GLIB_CHECK_VERSION (2, 63, 3)
void g_clear_slist (GSList ** slist_ptr, GDestroyNotify destroy);
void g_clear_list (GList ** list_ptr, GDestroyNotify destroy);
#endif /* ! GLIB_CHECK_VERSION (2, 63, 3) */

#if ! GLIB_CHECK_VERSION (2, 60, 0)
void g_queue_clear_full (GQueue * queue, GDestroyNotify free_func);
#endif /* ! GLIB_CHECK_VERSION (2, 60, 0) */

#if ! GLIB_CHECK_VERSION (2, 77, 0)
GString *g_string_new_take (char *init);
#endif /* ! GLIB_CHECK_VERSION (2, 77, 0) */

/* There is no such API in GLib2 */
GString *mc_g_string_copy (GString * dest, const GString * src);

/* There is no such API in GLib2 */
GString *mc_g_string_dup (const GString * s);

/* There is no such API in GLib2 */
GString *mc_g_string_append_c_len (GString * s, gchar c, guint len);

/*** inline functions ****************************************************************************/

#endif /* MC_GLIBCOMPAT_H */
