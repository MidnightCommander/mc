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

int display_box (WPanel * p, char **user, char **mini, int *use_msformat, int num);
const panel_field_t *sort_box (panel_sort_info_t *info);
void confirm_box (void);
void display_bits_box (void);
void configure_vfs (void);
void configure_vfs_plugin (void);
void jobs_cmd (void);
char *cd_dialog (void);
void symlink_dialog (const char *existing, const char *new, char **ret_existing, char **ret_new);
char *tree_box (const char *current_dir);

/*** inline functions ****************************************************************************/
#endif /* MC__BOXES_H */
