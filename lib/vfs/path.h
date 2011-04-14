#ifndef MC__VFS_PATH_H
#define MC__VFS_PATH_H

/*** typedefs(not structures) and defined constants **********************************************/

//typedef GPtrArray vfs_path_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct vfs_class;

typedef struct {
    GPtrArray *path;
    char *unparsed;
} vfs_path_t;

typedef struct {
    char *path;
    struct vfs_class *class;
    char *encoding;
#ifdef ENABLE_VFS_NET
    vfs_url_t *url;
#endif                          /* ENABLE_VFS_NET */
} vfs_path_element_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

vfs_path_t *vfs_path_new(void);
void vfs_path_free (vfs_path_t * path);
size_t vfs_path_length (const vfs_path_t *path);

char *vfs_path_to_str (const vfs_path_t * path);
vfs_path_t * vfs_path_from_str (const char * path_str);

vfs_path_element_t *vfs_path_get_by_index (const vfs_path_t *path, size_t element_index);
void vfs_path_element_free(vfs_path_element_t *element);


/*** inline functions ****************************************************************************/

#endif
