/*
 * filenot.c:  wrapper for routines to notify the
 * tree about the changes made to the directory
 * structure.
 *
 * Author:
 *    Janne Kukonlehto
 *    Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <malloc.h>
#include "util.h"
#include <errno.h>
#include "../vfs/vfs.h"

static char *get_absolute_name (char *file)
{
    char dir [MC_MAXPATHLEN];

    if (file [0] == PATH_SEP)
	return strdup (file);
    mc_get_current_wd (dir, MC_MAXPATHLEN);
    return get_full_name (dir, file);
}

static int
my_mkdir_rec (char *s, mode_t mode)
{
    char *p, *q;
    int result;
    
    if (!mc_mkdir (s, mode))
        return 0;
    else if (errno != ENOENT)
	return -1;

    /* FIXME: should check instead if s is at the root of that filesystem */
    if (!vfs_file_is_local (s))
	return -1;

    if (!strcmp (s, PATH_SEP_STR)) {
        errno = ENOTDIR;
        return -1;
    }

    p = concat_dir_and_file (s, "..");
    q = vfs_canon (p);
    free (p);

    if (!(result = my_mkdir_rec (q, mode))) 
    	result = mc_mkdir (s, mode);

    free (q);
    return result;
}

int
my_mkdir (char *s, mode_t mode)
{
    int result;

    result = mc_mkdir (s, mode);
#ifdef OS2_NT
    /* .ado: it will be disabled in OS/2 and NT */
    /* otherwise crash if directory already exists. */
    return result;
#endif
    if (result) {
        char *p = vfs_canon (s);
        
        result = my_mkdir_rec (p, mode);
        free (p);
    }
    if (result == 0){
	s = get_absolute_name (s);

#if FIXME
	tree_add_entry (tree, s);
#endif

	free (s);
    }
    return result;
}

int my_rmdir (char *s)
{
    int result;
#if FIXME    
    WTree *tree = 0;
#endif    

    /* FIXME: Should receive a Wtree! */
    result = mc_rmdir (s);
    if (result == 0){
	s = get_absolute_name (s);

#if FIXME
	tree_remove_entry (tree, s);
#endif

	free (s);
    }
    return result;
}

