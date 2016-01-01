/** \file treestore.h
 *  \brief Header: tree store
 *
 *  Contains a storage of the file system tree representation.
 */

#ifndef MC__TREE_STORE_H
#define MC__TREE_STORE_H

/*** typedefs(not structures) and defined constants **********************************************/

/*
 * Register/unregister notification functions for "entry_remove"
 */
struct tree_entry;
typedef void (*tree_store_remove_fn) (struct tree_entry * tree, void *data);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct tree_entry
{
    vfs_path_t *name;           /* The full path of directory */
    int sublevel;               /* Number of parent directories (slashes) */
    long submask;               /* Bitmask of existing sublevels after this entry */
    const char *subname;        /* The last part of name (the actual name) */
    gboolean mark;              /* Flag: Is this entry marked (e. g. for delete)? */
    gboolean scanned;           /* Flag: childs scanned or not */
    struct tree_entry *next;    /* Next item in the list */
    struct tree_entry *prev;    /* Previous item in the list */
} tree_entry;

struct TreeStore
{
    tree_entry *tree_first;     /* First entry in the list */
    tree_entry *tree_last;      /* Last entry in the list */
    tree_entry *check_start;    /* Start of checked subdirectories */
    vfs_path_t *check_name;
    GList *add_queue_vpath;     /* List of vfs_path_t objects of added directories */
    gboolean loaded;
    gboolean dirty;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

struct TreeStore *tree_store_get (void);
int tree_store_load (void);
int tree_store_save (void);
void tree_store_remove_entry (const vfs_path_t * name_vpath);
tree_entry *tree_store_start_check (const vfs_path_t * vpath);
void tree_store_mark_checked (const char *subname);
void tree_store_end_check (void);
tree_entry *tree_store_whereis (const vfs_path_t * name);
tree_entry *tree_store_rescan (const vfs_path_t * vpath);

void tree_store_add_entry_remove_hook (tree_store_remove_fn callback, void *data);
void tree_store_remove_entry_remove_hook (tree_store_remove_fn callback);

/*** inline functions ****************************************************************************/
#endif /* MC__TREE_STORE_H */
