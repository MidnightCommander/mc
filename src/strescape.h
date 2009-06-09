#ifndef MC__STRUTILS_ESCAPE_H
#define MC__STRUTILS_ESCAPE_H

#include <config.h>

#include "../src/global.h"      /* <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

char *strutils_escape (const char *, int, const char *, gboolean);
char *strutils_unescape (const char *, int, const char *, gboolean);

char *strutils_shell_unescape (const char *);
char *strutils_shell_escape (const char *);

char *strutils_glob_escape (const char *);
char *strutils_glob_escape (const char *);

char *strutils_regex_escape (const char *);
char *strutils_regex_escape (const char *);

gboolean strutils_is_char_escaped (const char *, const char *);

#endif
