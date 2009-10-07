
/** \file  ext.h
 *  \brief Header: extension dependent execution
 */

#ifndef MC_EXT_H
#define MC_EXT_H

int regex_command (const char *filename, const char *action, int *move_dir);

/* Call it after the user has edited the mc.ext file,
 * to flush the cached mc.ext file
 */
void flush_extension_file (void);

#endif
