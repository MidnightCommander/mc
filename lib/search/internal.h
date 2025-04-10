#ifndef MC__SEARCH_INTERNAL_H
#define MC__SEARCH_INTERNAL_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    COND__NOT_FOUND,
    COND__NOT_ALL_FOUND,
    COND__FOUND_CHAR,
    COND__FOUND_CHAR_LAST,
    COND__FOUND_OK,
    COND__FOUND_ERROR
} mc_search__found_cond_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_search_cond_struct
{
    GString *str;
    GString *upper;
    GString *lower;
    GRegex *regex_handle;
    gchar *charset;
} mc_search_cond_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* search/lib.c : */

GString *mc_search__recode_str (const char *str, gsize str_len, const char *charset_from,
                                const char *charset_to);
GString *mc_search__get_one_symbol (const char *charset, const char *str, gsize str_len,
                                    gboolean *just_letters);
GString *mc_search__tolower_case_str (const char *charset, const GString *str);
GString *mc_search__toupper_case_str (const char *charset, const GString *str);

/* search/regex.c : */

void mc_search__cond_struct_new_init_regex (const char *charset, mc_search_t *lc_mc_search,
                                            mc_search_cond_t *mc_search_cond);
gboolean mc_search__run_regex (mc_search_t *lc_mc_search, const void *user_data, off_t start_search,
                               off_t end_search, gsize *found_len);
GString *mc_search_regex_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str);

/* search/normal.c : */

void mc_search__cond_struct_new_init_normal (const char *charset, mc_search_t *lc_mc_search,
                                             mc_search_cond_t *mc_search_cond);
gboolean mc_search__run_normal (mc_search_t *lc_mc_search, const void *user_data,
                                off_t start_search, off_t end_search, gsize *found_len);
GString *mc_search_normal_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str);

/* search/glob.c : */

void mc_search__cond_struct_new_init_glob (const char *charset, mc_search_t *lc_mc_search,
                                           mc_search_cond_t *mc_search_cond);
gboolean mc_search__run_glob (mc_search_t *lc_mc_search, const void *user_data, off_t start_search,
                              off_t end_search, gsize *found_len);
GString *mc_search_glob_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str);

/* search/hex.c : */

void mc_search__cond_struct_new_init_hex (const char *charset, mc_search_t *lc_mc_search,
                                          mc_search_cond_t *mc_search_cond);
gboolean mc_search__run_hex (mc_search_t *lc_mc_search, const void *user_data, off_t start_search,
                             off_t end_search, gsize *found_len);
GString *mc_search_hex_prepare_replace_str (mc_search_t *lc_mc_search, GString *replace_str);

/*** inline functions ****************************************************************************/

#endif
