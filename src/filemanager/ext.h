/** \file  ext.h
 *  \brief Header: extension dependent execution
 */

#ifndef MC__EXT_H
#define MC__EXT_H
/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int regex_command_for (void *target, const vfs_path_t * filename_vpath, const char *action,
                       vfs_path_t ** script_vpath);

/* Call it after the user has edited the mc.ext file,
 * to flush the cached mc.ext file
 */
void flush_extension_file (void);

/*** inline functions ****************************************************************************/

static inline int
regex_command (const vfs_path_t *filename_vpath, const char *action)
{
    return regex_command_for (NULL, filename_vpath, action, NULL);
}

#endif /* MC__EXT_H */
