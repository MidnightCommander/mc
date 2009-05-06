#ifndef MC__SEARCH_INTERNAL_H
#define MC__SEARCH_INTERNAL_H

/*** typedefs(not structures) and defined constants **********************************************/

#if GLIB_CHECK_VERSION (2, 14, 0)
#define mc_search_regex_t GRegex
#else
#	if HAVE_LIBPCRE
#		define mc_search_regex_t pcre
#	else
#		define mc_search_regex_t regex_t
#	endif
#endif

/*** enums ***************************************************************************************/

typedef enum {
    COND__NOT_FOUND,
    COND__NOT_ALL_FOUND,
    COND__FOUND_CHAR,
    COND__FOUND_CHAR_LAST,
    COND__FOUND_OK,
    COND__FOUND_ERROR
} mc_search__found_cond_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_search_cond_struct {
    GString *str;
    GString *upper;
    GString *lower;
    mc_search_regex_t *regex_handle;
    gsize len;
    gchar *charset;
} mc_search_cond_t;

/*** global variables defined in .c file *********************************************************/

extern char STR_E_NOTFOUND[];
extern char STR_E_UNKNOWN_TYPE[];
extern char STR_E_RPL_NOT_EQ_TO_FOUND[];
extern char STR_E_RPL_INVALID_TOKEN[];
/*** declarations of public functions ************************************************************/


/* search/lib.c : */

gchar *mc_search__recode_str (const char *, gsize, const char *, const char *, gsize *);

gchar *mc_search__get_one_symbol (const char *, const char *, gsize, gboolean *);

int mc_search__get_char (mc_search_t *, const void *, gsize);

GString *mc_search__tolower_case_str (const char *, const char *, gsize);

GString *mc_search__toupper_case_str (const char *, const char *, gsize);

gboolean mc_search__regex_is_char_escaped (const char *, const char *);

/* search/regex.c : */

void mc_search__cond_struct_new_init_regex (const char *, mc_search_t *, mc_search_cond_t *);

gboolean mc_search__run_regex (mc_search_t *, const void *, gsize, gsize, gsize *);

GString *mc_search_regex_prepare_replace_str (mc_search_t *, GString *);

/* search/normal.c : */

void mc_search__cond_struct_new_init_normal (const char *, mc_search_t *, mc_search_cond_t *);

gboolean mc_search__run_normal (mc_search_t *, const void *, gsize, gsize, gsize *);

/* search/glob.c : */

void mc_search__cond_struct_new_init_glob (const char *, mc_search_t *, mc_search_cond_t *);

gboolean mc_search__run_glob (mc_search_t *, const void *, gsize, gsize, gsize *);

GString *mc_search_glob_prepare_replace_str (mc_search_t *, GString *);

/* search/hex.c : */

void mc_search__cond_struct_new_init_hex (const char *, mc_search_t *, mc_search_cond_t *);

gboolean mc_search__run_hex (mc_search_t *, const void *, gsize, gsize, gsize *);

GString *mc_search_hex_prepare_replace_str (mc_search_t *, GString *);

#endif
