#ifndef MC__VFS_PATH_H
#define MC__VFS_PATH_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct vfs_class;
struct vfs_url_struct;

typedef struct
{
    GList *path;
} vfs_path_t;

typedef struct
{
    char *path;
    struct vfs_class *class;
    char *encoding;

    struct
    {
        GIConv converter;
        DIR *info;
    } dir;

    char *raw_url_str;
    struct vfs_url_struct *url;

    struct vfs_s_super *current_super_block;
} vfs_path_element_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

vfs_path_t *vfs_path_new (void);
void vfs_path_free (vfs_path_t * path);
int vfs_path_elements_count (const vfs_path_t * path);

char *vfs_path_to_str (const vfs_path_t * path);
char *vfs_path_to_str_elements_count (const vfs_path_t * path, int elements_count);
vfs_path_t *vfs_path_from_str (const char *path_str);

vfs_path_element_t *vfs_path_get_by_index (const vfs_path_t * path, int element_index);
void vfs_path_element_free (vfs_path_element_t * element);

struct vfs_class *vfs_prefix_to_class (const char *prefix);

gboolean vfs_path_element_need_cleanup_converter (const vfs_path_element_t * element);

/*** inline functions ****************************************************************************/

#endif
