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
extern int xtree_mode;

struct Dlg_head;

/*** declarations of public functions ************************************************************/

WTree *tree_new (int y, int x, int lines, int cols, gboolean is_panel);

void tree_chdir (WTree * tree, const char *dir);
char *tree_selected_name (const WTree * tree);

void sync_tree (const char *pathname);

WTree *find_tree (struct Dlg_head *h);

/*** inline functions ****************************************************************************/
#endif /* MC__TREE_H */
