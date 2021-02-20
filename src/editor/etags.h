#ifndef MC__EDIT_ETAGS_H
#define MC__EDIT_ETAGS_H 1

/*** typedefs(not structures) and defined constants **********************************************/

#define MAX_WIDTH_DEF_DIALOG 60 /* max width def dialog */

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct etags_hash_struct
{
    char *filename;
    char *fullpath;
    char *short_define;
    long line;
} etags_hash_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/


GPtrArray *etags_set_definition_hash (const char *tagfile, const char *start_path,
                                      const char *match_func);

/*** inline functions ****************************************************************************/

#endif /* MC__EDIT_ETAGS_H */
