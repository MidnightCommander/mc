#ifndef MC_UTIL_H
#define MC_UTIL_H

#include <sys/types.h>

/* String managing functions */

extern const char *cstrcasestr (const char *haystack, const char *needle);

void str_replace(char *s, char from, char to);
int  is_printable (int c);
int  msglen (const char *text, int *lines);

/* Copy from s to d, and trim the beginning if necessary, and prepend
 * "..." in this case.  The destination string can have at most len
 * bytes, not counting trailing 0. */
char *trim (const char *s, char *d, int len);

/* Quote the filename for the purpose of inserting it into the command
 * line.  If quote_percent is 1, replace "%" with "%%" - the percent is
 * processed by the mc command line. */
char *name_quote (const char *c, int quote_percent);

/* returns a duplicate of c. */
char *fake_name_quote (const char *c, int quote_percent);

/* Remove the middle part of the string to fit given length.
 * Use "~" to show where the string was truncated.
 * Return static buffer, no need to free() it. */
const char *name_trunc (const char *txt, int trunc_len);

/* path_trunc() is the same as name_trunc() above but
 * it deletes possible password from path for security
 * reasons. */
const char *path_trunc (const char *path, int trunc_len);

/* return a static string representing size, appending "K" or "M" for
 * big sizes.
 * NOTE: uses the same static buffer as size_trunc_sep. */
const char *size_trunc (double size);

/* return a static string representing size, appending "K" or "M" for
 * big sizes. Separates every three digits by ",".
 * NOTE: uses the same static buffer as size_trunc. */
const char *size_trunc_sep (double size);

/* Print file SIZE to BUFFER, but don't exceed LEN characters,
 * not including trailing 0. BUFFER should be at least LEN+1 long.
 *
 * Units: size units (0=bytes, 1=Kbytes, 2=Mbytes, etc.) */
void size_trunc_len (char *buffer, int len, off_t size, int units);
int  is_exe (mode_t mode);
const char *string_perm (mode_t mode_bits);

/* @modifies path. @returns pointer into path. */
char *strip_password (char *path, int has_prefix);

/* @returns a pointer into a static buffer. */
const char *strip_home_and_password (const char *dir);

const char *extension (const char *);
char *concat_dir_and_file (const char *dir, const char *file);
const char *unix_error_string (int error_num);
const char *skip_separators (const char *s);
const char *skip_numbers (const char *s);
char *strip_ctrl_codes (char *s);

/* Replaces "\\E" and "\\e" with "\033". Replaces "^" + [a-z] with
 * ((char) 1 + (c - 'a')). The same goes for "^" + [A-Z].
 * Returns a newly allocated string. */
char *convert_controls (const char *s);

/* overwrites passwd with '\0's and frees it. */
void wipe_password (char *passwd);

char *diff_two_paths (const char *first, const char *second);

/* Returns the basename of fname. The result is a pointer into fname. */
const char *x_basename (const char *fname);

/* Profile managing functions */
int set_int (const char *, const char *, int);
int get_int (const char *, const char *, int);

char *load_file (const char *filename);
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
const char *file_date (time_t);

int exist_file (const char *name);

/* Returns a copy of *s until a \n is found and is below top */
const char *extract_line (const char *s, const char *top);
const char *_icase_search (const char *text, const char *data, int *lng);
#define icase_search(T,D) _icase_search((T), (D), NULL)

/* Matching */
enum {
    match_file,			/* match a filename, use easy_patterns */
    match_normal,		/* match pattern, use easy_patterns */
    match_regex			/* match pattern, force using regex */
};

extern int easy_patterns;
char *convert_pattern (const char *pattern, int match_type, int do_group);
int regexp_match (const char *pattern, const char *string, int match_type);

/* Error pipes */
void open_error_pipe (void);
void check_error_pipe (void);
int close_error_pipe (int error, const char *text);

/* Process spawning */
int my_system (int flags, const char *shell, const char *command);
void save_stop_handler (void);
extern struct sigaction startup_handler;

/* Tilde expansion */
char *tilde_expand (const char *);

/* Pathname canonicalization */
void canonicalize_pathname (char *);

/* Misc Unix functions */
char *get_current_wd (char *buffer, int size);
int my_mkdir (const char *s, mode_t mode);
int my_rmdir (const char *s);

/* Rotating dash routines */
void use_dash (int flag); /* Disable/Enable rotate_dash routines */
void rotate_dash (void);

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
void load_file_position (const char *filename, long *line, long *column);
/* Save position for the given filename */
void save_file_position (const char *filename, long line, long column);


/* OS specific defines */
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#define PATH_ENV_SEP ':'
#define TMPDIR_DEFAULT "/tmp"
#define SCRIPT_SUFFIX ""
#define get_default_editor() "vi"
#define OS_SORT_CASE_SENSITIVE_DEFAULT 1
#define STRCOMP strcmp
#define STRNCOMP strncmp
#define MC_ARCH_FLAGS 0

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

/* this function allows you to write:
 * char *s = g_strdup("hello, world");
 * s = free_after(g_strconcat(s, s, (char *) NULL), s);
 */
static inline char *
free_after (char *result, char *string_to_free)
{
	g_free(string_to_free);
	return result;
}

#endif
