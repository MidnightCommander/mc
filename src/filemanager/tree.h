/** \file tree.h
 *  \brief Header: directory tree browser
 */

#ifndef MC__TREE_H
#define MC__TREE_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WTree WTree;

typedef struct
{
    WTree *tree;
    char *dir;
} mc_tree_chdir_t;

/*** global variables defined in .c file *********************************************************/

extern WTree *the_tree;
extern int xtree_mode;

struct WDialog;

/*** declarations of public functions ************************************************************/

WTree *tree_new (int y, int x, int lines, int cols, gboolean is_panel);

vfs_path_t *tree_selected_name (const WTree * tree);

/*** inline functions ****************************************************************************/
#endif /* MC__TREE_H */
