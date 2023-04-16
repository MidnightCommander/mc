/** \file lib/file-entry.h
 *  \brief Header: file entry definition
 */

#ifndef MC__ILE_ENTRY_H
#define MC__ILE_ENTRY_H

#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"         /* include <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* keys are set only during sorting */
typedef struct
{
    /* File name */
    GString *fname;
    /* File attributes */
    struct stat st;
    /* Key used for comparing names */
    char *name_sort_key;
    /* Key used for comparing extensions */
    char *extension_sort_key;

    /* Flags */
    struct
    {
        unsigned int marked:1;  /* File marked in pane window */
        unsigned int link_to_dir:1;     /* If this is a link, does it point to directory? */
        unsigned int stale_link:1;      /* If this is a symlink and points to Charon's land */
        unsigned int dir_size_computed:1;       /* Size of directory was computed with dirsizes_cmd */
    } f;
} file_entry_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#endif /* MC__FILE_ENTRY_H */
