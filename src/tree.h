
/** \file tree.h
 *  \brief Header: directory tree browser
 */

#ifndef MC_TREE_H
#define MC_TREE_H

typedef struct WTree WTree;

extern WTree *the_tree;
extern int xtree_mode;

WTree *tree_new (int is_panel, int y, int x, int lines, int cols);

void tree_chdir (WTree *tree, const char *dir);
char *tree_selected_name (const WTree *tree);

void sync_tree (const char *pathname);

#endif					/* MC_TREE_H */
