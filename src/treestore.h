#ifndef __TREE_STORE_H
#define __TREE_STORE_H

typedef struct tree_entry {
	char *name;			/* The full path of directory */
	int sublevel;			/* Number of parent directories (slashes) */
	long submask;			/* Bitmask of existing sublevels after this entry */
	char *subname;			/* The last part of name (the actual name) */
	unsigned int mark:1;		/* Flag: Is this entry marked (e. g. for delete)? */
	unsigned int scanned:1;		/* Flag: childs scanned or not */
	struct tree_entry *next;	/* Next item in the list */
	struct tree_entry *prev;	/* Previous item in the list */
} tree_entry;

typedef struct {
	struct tree_entry *base;
	struct tree_entry *current;
	int base_dir_len;
	int sublevel;
} tree_scan;

typedef struct {
	int refcount;
	tree_entry *tree_first;     	/* First entry in the list */
	tree_entry *tree_last;          /* Last entry in the list */
	tree_entry *check_start;	/* Start of checked subdirectories */
	char       *check_name;		/* Directory which is been checked */
	unsigned int loaded : 1;
	unsigned int dirty : 1;
} TreeStore;

extern void (*tree_store_dirty_notify)(int state);

TreeStore  *tree_store_init            (void);
int         tree_store_load            (char *name);
int         tree_store_save            (char *name);
tree_entry *tree_store_add_entry       (char *name);
void        tree_store_remove_entry    (char *name);
void        tree_store_destroy         (void);
tree_entry *tree_store_start_check     (char *path);
tree_entry *tree_store_start_check_cwd (void);
void        tree_store_mark_checked    (const char *subname);
void        tree_store_end_check       (void);
tree_entry *tree_store_whereis         (char *name);
tree_entry *tree_store_rescan          (char *dir);

typedef void (*tree_store_remove_fn)(tree_entry *tree, void *data);
void        tree_store_add_entry_remove_hook (tree_store_remove_fn callback, void *data);

void        tree_store_notify_remove (tree_entry *entry);

tree_scan  *tree_store_opendir       (char *path);
tree_entry *tree_store_readdir       (tree_scan *scanner);
void        tree_store_closedir      (tree_scan *scanner);

#endif
