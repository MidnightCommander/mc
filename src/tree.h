#ifndef __TREE_H
#define __TREE_H

#include "treestore.h"

#include "dlg.h"
typedef struct {
    Widget     widget;
    TreeStore  *store;
    tree_entry *selected_ptr;	        /* The selected directory */
    char       search_buffer [256];     /* Current search string */
    int        done;		        /* Flag: exit tree */
    tree_entry **tree_shown;	        /* Entries currently on screen */
    int        is_panel;		/* panel or plain widget flag */
    int        active;		        /* if it's currently selected */
    int        searching;	        /* Are we on searching mode? */
    int topdiff;    			/* The difference between the topmost shown and the selected */
} WTree;

#define tlines(t) (t->is_panel ? t->widget.lines-2 - (show_mini_info ? 2 : 0) : t->widget.lines)

int tree_init (char *current_dir, int lines);
void load_tree (WTree *tree);
void save_tree (WTree *tree);
void show_tree (WTree *tree);
void tree_chdir (WTree *tree, char *dir);
void tree_rescan_cmd (WTree *tree);
int tree_forget_cmd (WTree *tree);
void tree_copy (WTree *tree, char *default_dest);
void tree_move (WTree *tree, char *default_dest);
void tree_event (WTree *tree, int y);
char *tree (char *current_dir);

int search_tree (WTree *tree, char *text);

tree_entry *tree_add_entry (WTree *tree, char *name);
void tree_remove_entry (WTree *tree, char *name);
void tree_destroy (WTree *tree);

void tree_move_backward (WTree *tree, int i);
void tree_move_forward (WTree *tree, int i);
int tree_move_to_parent (WTree *tree);
void tree_move_to_child (WTree *tree);
void tree_move_to_top (WTree *tree);
void tree_move_to_bottom (WTree *tree);

void sync_tree (char *pathname);

extern int tree_navigation_flag;
extern int xtree_mode;

WTree *tree_new (int is_panel, int y, int x, int lines, int cols);
extern WTree *the_tree;

#endif
