#ifndef __TREE_STORE_H
#define __TREE_STORE_H

typedef struct tree_entry {
	char *name;			/* The full path of directory */
	int sublevel;			/* Number of parent directories (slashes) */
	long submask;			/* Bitmask of existing sublevels after this entry */
	char *subname;			/* The last part of name (the actual name) */
	int mark;			/* Flag: Is this entry marked (e. g. for delete)? */
	struct tree_entry *next;	/* Next item in the list */
	struct tree_entry *prev;	/* Previous item in the list */
} tree_entry;

typedef struct {
	int refcount;
	tree_entry *tree_first;     	/* First entry in the list */
	tree_entry *tree_last;          /* Last entry in the list */
	tree_entry *check_start;	/* Start of checked subdirectories */
	char       check_name [MC_MAXPATHLEN];/* Directory which is been checked */
	int        loaded;
} TreeStore;

TreeStore  *tree_store_init         (void);
int         tree_store_load         (char *name);
int         tree_store_save         (char *name);
tree_entry *tree_store_add_entry    (char *name);
void        tree_store_remove_entry (char *name);
void        tree_store_destroy      (void);
void        tree_store_start_check  (void);
void        tree_store_mark_checked (const char *subname);
void        tree_store_end_check    (void);
tree_entry *tree_store_whereis      (char *name);
void        tree_store_rescan       (char *dir);

typedef void (*tree_store_remove_fn)(tree_entry *tree, void *data);
void        tree_store_add_entry_remove_hook (tree_store_remove_fn callback, void *data);

#endif
