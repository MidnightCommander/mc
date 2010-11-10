#ifndef MC__EDIT_ETAGS_H
#define MC__EDIT_ETAGS_H 1

#include <sys/types.h>          /* size_t */
#include "lib/global.h"         /* include <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

#define MAX_WIDTH_DEF_DIALOG 60 /* max width def dialog */
#define MAX_DEFINITIONS 60      /* count found entries show */
#define SHORT_DEF_LEN   30
#define LONG_DEF_LEN    40
#define LINE_DEF_LEN    16

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct etags_hash_struct
{
    size_t filename_len;
    char *fullpath;
    char *filename;
    char *short_define;
    long line;
} etags_hash_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/


int etags_set_definition_hash (const char *tagfile, const char *start_path,
                               const char *match_func, etags_hash_t * def_hash);

/*** inline functions ****************************************************************************/
#endif /* MC__EDIT_ETAGS_H */
