#ifndef MC_GLIBCOMPAT_H
#define MC_GLIBCOMPAT_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

#if ! GLIB_CHECK_VERSION (2, 13, 0)
gboolean g_unichar_iszerowidth (gunichar);
#endif /* ! GLIB_CHECK_VERSION (2, 13, 0) */

/*** inline functions ****************************************************************************/

#endif /* MC_GLIBCOMPAT_H */
