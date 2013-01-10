/** \file lib/util.h
 *  \brief Header: various utilities
 */

#ifndef MC_UTIL_H
#define MC_UTIL_H

#include "lib/global.h"         /* include <glib.h> */

#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>           /* uintmax_t */
#include <unistd.h>

#include "lib/vfs/vfs.h"

/*** typedefs(not structures) and defined constants **********************************************/

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 32
#endif

#define MAX_SAVED_BOOKMARKS 10

#define MC_PTR_FREE(ptr) do { g_free (ptr); (ptr) = NULL; } while (0)

/*** enums ***************************************************************************************/

/* Pathname canonicalization */
typedef enum
{
    CANON_PATH_JOINSLASHES = 1L << 0,   /* Multiple `/'s are collapsed to a single `/'. */
    CANON_PATH_REMSLASHDOTS = 1L << 1,  /* Leading `./'s, `/'s and trailing `/.'s are removed. */
    CANON_PATH_REMDOUBLEDOTS = 1L << 3, /* Non-leading `../'s and trailing `..'s are handled by removing */
    CANON_PATH_GUARDUNC = 1L << 4,      /* Detect and preserve UNC paths: //server/... */
    CANON_PATH_ALL = CANON_PATH_JOINSLASHES
        | CANON_PATH_REMSLASHDOTS | CANON_PATH_REMDOUBLEDOTS | CANON_PATH_GUARDUNC
} CANON_PATH_FLAGS;

enum compression_type
{
    COMPRESSION_NONE,
    COMPRESSION_GZIP,
    COMPRESSION_BZIP,
    COMPRESSION_BZIP2,
    COMPRESSION_LZMA,
    COMPRESSION_XZ
};

/*** structures declarations (and typedefs of structures)*****************************************/

/* keys are set only during sorting */
typedef struct
{
    /* File attributes */
    size_t fnamelen;
    char *fname;
    struct stat st;
    /* key used for comparing names */
    char *sort_key;
    /* key used for comparing extensions */
    char *second_sort_key;

    /* Flags */
    struct
    {
        unsigned int marked:1;  /* File marked in pane window */
        unsigned int link_to_dir:1;     /* If this is a link, does it point to directory? */
        unsigned int stale_link:1;      /* If this is a symlink and points to Charon's land */
        unsigned int dir_size_computed:1;       /* Size of directory was computed with dirsizes_cmd */
    } f;
} file_entry;

/*** global variables defined in .c file *********************************************************/

extern struct sigaction startup_handler;

/*** declarations of public functions ************************************************************/

int is_printable (int c);

/* Quote the filename for the purpose of inserting it into the command
 * line.  If quote_percent is 1, replace "%" with "%%" - the percent is
 * processed by the mc command line. */
char *name_quote (const char *c, int quote_percent);

/* returns a duplicate of c. */
char *fake_name_quote (const char *c, int quote_percent);

/* path_trunc() is the same as str_trunc() but
 * it deletes possible password from path for security
 * reasons. */
const char *path_trunc (const char *path, size_t trunc_len);

/* return a static string representing size, appending "K" or "M" for
 * big sizes.
 * NOTE: uses the same static buffer as size_trunc_sep. */
const char *size_trunc (uintmax_t size, gboolean use_si);

/* return a static string representing size, appending "K" or "M" for
 * big sizes. Separates every three digits by ",".
 * NOTE: uses the same static buffer as size_trunc. */
const char *size_trunc_sep (uintmax_t size, gboolean use_si);

/* Print file SIZE to BUFFER, but don't exceed LEN characters,
 * not including trailing 0. BUFFER should be at least LEN+1 long.
 *
 * Units: size units (0=bytes, 1=Kbytes, 2=Mbytes, etc.) */
void size_trunc_len (char *buffer, unsigned int len, uintmax_t size, int units, gboolean use_si);
const char *string_perm (mode_t mode_bits);

const char *extension (const char *);
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

char *diff_two_paths (const vfs_path_t * vpath1, const vfs_path_t * vpath2);

/* Returns the basename of fname. The result is a pointer into fname. */
const char *x_basename (const char *fname);

char *load_mc_home_file (const char *from, const char *filename, char **allocated_filename);

/* uid/gid managing */
void init_groups (void);
void destroy_groups (void);
int get_user_permissions (struct stat *buf);

void init_uid_gid_cache (void);
char *get_group (int);
char *get_owner (int);

/* Returns a copy of *s until a \n is found and is below top */
const char *extract_line (const char *s, const char *top);

/* Error pipes */
void open_error_pipe (void);
void check_error_pipe (void);
int close_error_pipe (int error, const char *text);

/* Process spawning */
int my_system (int flags, const char *shell, const char *command);
int my_systeml (int flags, const char *shell, ...);
int my_systemv (const char *command, char *const argv[]);
int my_systemv_flags (int flags, const char *command, char *const argv[]);

void my_exit (int status);
void save_stop_handler (void);

/* Tilde expansion */
char *tilde_expand (const char *);

void custom_canonicalize_pathname (char *, CANON_PATH_FLAGS);
void canonicalize_pathname (char *);

#ifdef HAVE_REALPATH
#define mc_realpath realpath
#else
char *mc_realpath (const char *path, char *resolved_path);
#endif

/* Looks for ``magic'' bytes at the start of the VFS file to guess the
 * compression type. Side effect: modifies the file position. */
enum compression_type get_compression_type (int fd, const char *);
const char *decompress_extension (int type);

GList *list_append_unique (GList * list, char *text);

/* Position saving and restoring */
/* Load position for the given filename */
void load_file_position (const vfs_path_t * filename_vpath, long *line, long *column,
                         off_t * offset, GArray ** bookmarks);
/* Save position for the given filename */
void save_file_position (const vfs_path_t * filename_vpath, long line, long column, off_t offset,
                         GArray * bookmarks);


/* if ch is in [A-Za-z], returns the corresponding control character,
 * else returns the argument. */
extern int ascii_alpha_to_cntrl (int ch);

#undef Q_
const char *Q_ (const char *s);

gboolean mc_util_make_backup_if_possible (const char *, const char *);
gboolean mc_util_restore_from_backup_if_possible (const char *, const char *);
gboolean mc_util_unlink_backup_if_possible (const char *, const char *);

char *guess_message_value (void);

char *mc_build_filename (const char *first_element, ...);
char *mc_build_filenamev (const char *first_element, va_list args);

/*** inline functions **************************************************/

static inline gboolean
exist_file (const char *name)
{
    return (access (name, R_OK) == 0);
}

static inline gboolean
is_exe (mode_t mode)
{
    return (gboolean) ((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode));
}

#endif /* MC_UTIL_H */
