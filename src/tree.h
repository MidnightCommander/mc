#ifndef MC_TREE_H
#define MC_TREE_H

struct WTree;
typedef struct WTree WTree;

int tree_init (const char *current_dir, int lines);
void tree_chdir (WTree *tree, const char *dir);
char *tree_selected_name (WTree *tree);

void sync_tree (const char *pathname);

extern int xtree_mode;

WTree *tree_new (int is_panel, int y, int x, int lines, int cols);
extern WTree *the_tree;

#endif
