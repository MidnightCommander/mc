
/** \file treestore.h
 *  \brief Header: tree store
 *
 *  Contains a storage of the file system tree representation.
 */

#ifndef MC_TREE_STORE_H
#define MC_TREE_STORE_H

typedef struct tree_entry {
    char *name;			/* The full path of directory */
    int sublevel;		/* Number of parent directories (slashes) */
    long submask;		/* Bitmask of existing sublevels after this entry */
    const char *subname;		/* The last part of name (the actual name) */
    unsigned int mark:1;	/* Flag: Is this entry marked (e. g. for delete)? */
    unsigned int scanned:1;	/* Flag: childs scanned or not */
    struct tree_entry *next;	/* Next item in the list */
    struct tree_entry *prev;	/* Previous item in the list */
} tree_entry;

struct TreeStore {
    tree_entry *tree_first;	/* First entry in the list */
    tree_entry *tree_last;	/* Last entry in the list */
    tree_entry *check_start;	/* Start of checked subdirectories */
    char *check_name;
    GList *add_queue;		/* List of strings of added directories */
    unsigned int loaded:1;
    unsigned int dirty:1;
};

struct TreeStore *tree_store_get (void);
int tree_store_load (void);
int tree_store_save (void);
void tree_store_remove_entry (const char *name);
tree_entry *tree_store_start_check (const char *path);
void tree_store_mark_checked (const char *subname);
void tree_store_end_check (void);
tree_entry *tree_store_whereis (const char *name);
tree_entry *tree_store_rescan (const char *dir);

/*
 * Register/unregister notification functions for "entry_remove"
 */
typedef void (*tree_store_remove_fn) (tree_entry *tree, void *data);
void tree_store_add_entry_remove_hook (tree_store_remove_fn callback,
				       void *data);
void tree_store_remove_entry_remove_hook (tree_store_remove_fn callback);

#endif
