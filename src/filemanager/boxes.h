/** \file boxes.h
 *  \brief Header: Some misc dialog boxes for the program
 */

#ifndef MC__BOXES_H
#define MC__BOXES_H

#include "dir.h"
#include "panel.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int panel_listing_box (WPanel * p, char **user, char **mini, int *use_msformat, int num);
const panel_field_t *sort_box (dir_sort_options_t * op, const panel_field_t * sort_field);
char *cd_dialog (void);
void symlink_dialog (const vfs_path_t * existing_vpath, const vfs_path_t * new_vpath,
                     char **ret_existing, char **ret_new);

/*** inline functions ****************************************************************************/
#endif /* MC__BOXES_H */
