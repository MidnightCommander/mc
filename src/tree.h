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
void tree_chdir (WTree *tree, char *dir);

void sync_tree (char *pathname);

extern int xtree_mode;

WTree *tree_new (int is_panel, int y, int x, int lines, int cols);
extern WTree *the_tree;

#endif
