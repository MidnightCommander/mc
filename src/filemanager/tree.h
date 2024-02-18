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

/*** global variables defined in .c file *********************************************************/

extern WTree *the_tree;
extern gboolean xtree_mode;

/*** declarations of public functions ************************************************************/

WTree *tree_new (const WRect * r, gboolean is_panel);

void tree_chdir (WTree * tree, const vfs_path_t * dir);
const vfs_path_t *tree_selected_name (const WTree * tree);

void sync_tree (const vfs_path_t * vpath);

WTree *find_tree (const WDialog * h);

/*** inline functions ****************************************************************************/
#endif /* MC__TREE_H */
