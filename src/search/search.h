#ifndef MC__SEARCH_H
#define MC__SEARCH_H

#include <config.h>

#include "../src/global.h"      /* <glib.h> */

#if ! GLIB_CHECK_VERSION (2, 14, 0)
#	if HAVE_LIBPCRE
#		include <pcre.h>
#	else
#		include <stdio.h>
#		include <string.h>
#		include <sys/types.h>
#		include <regex.h>
#	endif
#endif

/*** typedefs(not structures) and defined constants **********************************************/

typedef int (*mc_search_fn) (const void *user_data, gsize char_offset);

#define MC_SEARCH__NUM_REPL_ARGS 64

#if GLIB_CHECK_VERSION (2, 14, 0)
#define mc_search_matchinfo_t GMatchInfo
#else
#	if HAVE_LIBPCRE
#		define mc_search_matchinfo_t pcre_extra
#	else
#		define mc_search_matchinfo_t regmatch_t
#	endif
#endif

#define MC_SEARCH__PCRE_MAX_MATCHES 64

/*** enums ***************************************************************************************/

typedef enum {
    MC_SEARCH_E_OK,
    MC_SEARCH_E_INPUT,
    MC_SEARCH_E_REGEX_COMPILE,
    MC_SEARCH_E_REGEX,
    MC_SEARCH_E_REGEX_REPLACE,
    MC_SEARCH_E_NOTFOUND
} mc_search_error_t;

typedef enum {
    MC_SEARCH_T_NORMAL,
    MC_SEARCH_T_REGEX,
    MC_SEARCH_T_HEX,
    MC_SEARCH_T_GLOB
} mc_search_type_t;


/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_search_struct {

/* public input data */

    /* search in all charsets */
    gboolean is_all_charsets;

    /* case sentitive search */
    gboolean is_case_sentitive;

    /* search only once.  Is this for replace? */
    gboolean is_once_only;

    /* function, used for getting data. NULL if not used */
    mc_search_fn search_fn;

    /* type of search */
    mc_search_type_t search_type;


/* public output data */

    /* some data for normal */
    gsize normal_offset;

    /* some data for regexp */
    mc_search_matchinfo_t *regex_match_info;
    GString *regex_buffer;
#if ! GLIB_CHECK_VERSION (2, 14, 0)
    int num_rezults;
#if HAVE_LIBPCRE
    int iovector[MC_SEARCH__PCRE_MAX_MATCHES * 2];
#else                           /* HAVE_LIBPCRE */
#endif                          /* HAVE_LIBPCRE */
#endif                          /* ! GLIB_CHECK_VERSION (2, 14, 0) */

/* private data */

    /* prepared conditions */
    GPtrArray *conditions;

    /* original search string */
    gchar *original;
    gsize original_len;

    /* error code after search */
    mc_search_error_t error;
    gchar *error_str;

} mc_search_t;

typedef struct mc_search_type_str_struct {
    char *str;
    mc_search_type_t type;
} mc_search_type_str_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

mc_search_t *mc_search_new (const gchar * original, gsize original_len);

void mc_search_free (mc_search_t * mc_search);

gboolean mc_search_run (mc_search_t * mc_search, const void *user_data, gsize start_search,
                        gsize end_search, gsize * founded_len);

gboolean mc_search_is_type_avail (mc_search_type_t);

const mc_search_type_str_t *mc_search_types_list_get (void);

GString *mc_search_prepare_replace_str (mc_search_t * mc_search, GString * replace_str);

gboolean mc_search_is_fixed_search_str (mc_search_t *);

gchar **mc_search_get_types_strings_array (void);

gboolean mc_search (const gchar *, const gchar *, mc_search_type_t);

int mc_search_getstart_rezult_by_num(mc_search_t *, int);
int mc_search_getend_rezult_by_num(mc_search_t *, int);

#endif
