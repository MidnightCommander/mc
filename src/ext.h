#ifndef __EXT_H
#define __EXT_H

char *regex_command (char *filename, char *action, char **drops, int *move_dir);
void exec_extension (char *filename, char *data, char **drops, int *move_dir, int start_line);

/* Call it after the user has edited the mc.ext file, 
 * to flush the cached mc.ext file
 */
void flush_extension_file (void);

#ifdef OS2_NT
#    define MC_USER_EXT "mc.ext"
#    define MC_LIB_EXT  "mc.ext"
#else
#    ifdef HAVE_GNOME
#        define MC_USER_EXT ".mc/gnome.ext" 
#        define MC_LIB_EXT  "mc-gnome.ext"
#    else
#        define MC_USER_EXT ".mc/bindings" 
#        define MC_LIB_EXT  "mc.ext"
#    endif
#endif
#endif
