#ifndef __TREE_H
#define __TREE_H

#include "dlg.h"

struct WTree;
typedef struct WTree WTree;

#define tlines(t) (t->is_panel ? t->widget.lines-2 - (show_mini_info ? 2 : 0) : t->widget.lines)

int tree_init (char *current_dir, int lines);
void tree_chdir (WTree *tree, char *dir);
char *tree_selected_name (WTree *tree);

void sync_tree (char *pathname);

extern int xtree_mode;

WTree *tree_new (int is_panel, int y, int x, int lines, int cols);
extern WTree *the_tree;

#endif
