
/** \file tree.h
 *  \brief Header: directory tree browser
 */

#ifndef MC_TREE_H
#define MC_TREE_H

#include "lib/global.h"

typedef struct WTree WTree;

extern WTree *the_tree;
extern int xtree_mode;

WTree *tree_new (int y, int x, int lines, int cols, gboolean is_panel);

void tree_chdir (WTree *tree, const char *dir);
char *tree_selected_name (const WTree *tree);

void sync_tree (const char *pathname);

struct Dlg_head;

WTree *find_tree (struct Dlg_head *h);

#endif /* MC_TREE_H */
