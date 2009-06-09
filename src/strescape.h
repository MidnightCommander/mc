#ifndef MC__STRUTILS_ESCAPE_H
#define MC__STRUTILS_ESCAPE_H

#include <config.h>

#include "../src/global.h"      /* <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean strutils_is_char_escaped (const char *);
char *strutils_shell_unescape (const char *);
char *strutils_shell_escape (const char *);

#endif
