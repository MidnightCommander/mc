#ifndef __EXT_H
#define __EXT_H

int regex_command (char *filename, char *action, int *move_dir);

/* Call it after the user has edited the mc.ext file, 
 * to flush the cached mc.ext file
 */
void flush_extension_file (void);

#ifdef NATIVE_WIN32
#    define MC_USER_EXT "mc.ext"
#    define MC_LIB_EXT  "mc.ext"
#else
#    define MC_USER_EXT ".mc/bindings" 
#    define MC_LIB_EXT  "mc.ext"
#endif
#endif
