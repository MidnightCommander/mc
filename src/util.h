#ifndef __UTIL_H
#define __UTIL_H


#include <sys/types.h>

/* String managing functions */

int  is_printable (int c);
int  msglen (char *text, int *lines);
char *trim (char *s, char *d, int len);
char *name_quote (const char *c, int quote_percent);
char *fake_name_quote (const char *c, int quote_percent);
char *name_trunc (const char *txt, int trunc_len);
char *size_trunc (double size);
char *size_trunc_sep (double size);
void size_trunc_len (char *buffer, int len, off_t size, int units);
int  is_exe (mode_t mode);
char *string_perm (mode_t mode_bits);
char *strip_password (char *path, int has_prefix);
char *strip_home_and_password (const char *dir);
char *extension (char *);
char *concat_dir_and_file (const char *dir, const char *file);
char *unix_error_string (int error_num);
char *skip_separators (char *s);
char *skip_numbers (char *s);
char *strip_ctrl_codes (char *s);
char *convert_controls (char *s);
void wipe_password (char *passwd);
char *diff_two_paths (char *first, char *second);

char *x_basename (char *s);

/* Profile managing functions */
int set_int (char *, char *, int);
int get_int (char *, char *, int);

char *load_file (char *filename);
char *load_mc_home_file (const char *filename, char ** allocated_filename);

/* uid/gid managing */
void init_groups (void);
void destroy_groups (void);
int get_user_permissions (struct stat *buf);

void init_uid_gid_cache (void);
char *get_group (int);
char *get_owner (int);

#define MAX_I18NTIMELENGTH 14
#define MIN_I18NTIMELENGTH 10
#define STD_I18NTIMELENGTH 12

size_t i18n_checktimelength (void);
char *file_date (time_t);

int exist_file (char *name);

/* Returns a copy of *s until a \n is found and is below top */
char *extract_line (char *s, char *top);
char *_icase_search (char *text, char *data, int *lng);
#define icase_search(T,D) _icase_search((T), (D), NULL)

/* Matching */
enum {
    match_file,			/* match a filename, use easy_patterns */
    match_normal,		/* match pattern, use easy_patterns */
    match_regex,		/* match pattern, force using regex */
};

extern int easy_patterns;
char *convert_pattern (char *pattern, int match_type, int do_group);
int regexp_match (char *pattern, char *string, int match_type);

/* Error pipes */
void open_error_pipe (void);
void check_error_pipe (void);
int close_error_pipe (int error, char *text);

/* Process spawning */
#define EXECUTE_INTERNAL   1
#define EXECUTE_AS_SHELL   4
int my_system (int flags, const char *shell, const char *command);
void save_stop_handler (void);
extern struct sigaction startup_handler;

/* Tilde expansion */
char *tilde_expand (const char *);

/* Pathname canonicalization */
char *canonicalize_pathname (char *);

/* Misc Unix functions */
char *get_current_wd (char *buffer, int size);
int my_mkdir (char *s, mode_t mode);
int my_rmdir (char *s);

/* Rotating dash routines */
void use_dash (int flag); /* Disable/Enable rotate_dash routines */
void rotate_dash (void);
void remove_dash (void);

/* Creating temporary files safely */
const char *mc_tmpdir (void);
int mc_mkstemps(char **pname, const char *prefix, const char *suffix);

enum {
	COMPRESSION_NONE,
	COMPRESSION_GZIP,
	COMPRESSION_BZIP,
	COMPRESSION_BZIP2
};

int get_compression_type (int fd);
const char *decompress_extension (int type);

int mc_doublepopen (int inhandle, int inlen, pid_t *tp, char *command, ...);
int mc_doublepclose (int pipehandle, pid_t pid);

/* Hook functions */

typedef struct hook {
    void (*hook_fn)(void *);
    void *hook_data;
    struct hook *next;
} Hook;

void add_hook (Hook **hook_list, void (*hook_fn)(void *), void *data);
void execute_hooks (Hook *hook_list);
void delete_hook (Hook **hook_list, void (*hook_fn)(void *));
int hook_present (Hook *hook_list, void (*hook_fn)(void *));

GList *list_append_unique (GList *list, char *text);

/* Position saving and restoring */

/* file where positions are stored */
#define MC_FILEPOS ".mc/filepos"
/* temporary file */
#define MC_FILEPOS_TMP ".mc/filepos.tmp"
/* maximum entries in MC_FILEPOS */
#define MC_FILEPOS_ENTRIES 1024
/* Load position for the given filename */
void load_file_position (char *filename, long *line, long *column);
/* Save position for the given filename */
void save_file_position (char *filename, long line, long column);


/* OS specific defines */

#ifdef NATIVE_WIN32
#    define PATH_SEP '\\'
#    define PATH_SEP_STR "\\"
#    define PATH_ENV_SEP ';'
#    define TMPDIR_DEFAULT "c:\\temp"
#    define SCRIPT_SUFFIX ".cmd"
#    define OS_SORT_CASE_SENSITIVE_DEFAULT 0
#    define STRCOMP stricmp
#    define STRNCOMP strnicmp
#    define MC_ARCH_FLAGS REG_ICASE
     char *get_default_shell (void);
     char *get_default_editor (void);
     int lstat (const char* pathname, struct stat *buffer);
#else
#    define PATH_SEP '/'
#    define PATH_SEP_STR "/"
#    define PATH_ENV_SEP ':'
#    define TMPDIR_DEFAULT "/tmp"
#    define SCRIPT_SUFFIX ""
#    define get_default_editor() "vi"
#    define OS_SORT_CASE_SENSITIVE_DEFAULT 1
#    define STRCOMP strcmp
#    define STRNCOMP strncmp
#    define MC_ARCH_FLAGS 0
#endif

#include "i18n.h"

/* taken from regex.c: */
/* Jim Meyering writes:

   "... Some ctype macros are valid only for character codes that
   isascii says are ASCII (SGI's IRIX-4.0.5 is one such system --when
   using /bin/cc or gcc but without giving an ansi option).  So, all
   ctype uses should be through macros like ISPRINT...  If
   STDC_HEADERS is defined, then autoconf has verified that the ctype
   macros don't need to be guarded with references to isascii. ...
   Defining isascii to 1 should let any compiler worth its salt
   eliminate the && through constant folding."  */

#if defined (STDC_HEADERS) || (!defined (isascii) && !defined (HAVE_ISASCII))
#define ISASCII(c) 1
#else
#define ISASCII(c) isascii(c)
#endif

#endif
