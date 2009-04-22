#ifndef MC__SEARCH_H
#define MC__SEARCH_H

/*** typedefs(not structures) and defined constants **********************************************/

typedef int (*mc_search_fn) (const void *user_data, gsize char_offset);

#define MC_SEARCH__NUM_REPL_ARGS 64
#define MC_SEARCH__MAX_REPL_LEN 1024

/*** enums ***************************************************************************************/

typedef enum {
    MC_SEARCH_E_OK,
    MC_SEARCH_E_INPUT,
    MC_SEARCH_E_REGEX,
    MC_SEARCH_E_NOTFOUND,
} mc_search_error_t;

typedef enum {
    MC_SEARCH_T_NORMAL,
    MC_SEARCH_T_REGEX,
    MC_SEARCH_T_SCANF,
    MC_SEARCH_T_HEX,
    MC_SEARCH_T_GLOB,
} mc_search_type_t;


/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_search_struct {

/* public input data */

    /* search in all charsets */
    gboolean is_all_charsets;

    /* case sentitive search */
    gboolean is_case_sentitive;

    /* backward search */
    gboolean is_backward;

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
    GRegex *regex_handle;
    GMatchInfo *regex_match_info;

    /* some data for sscanf */

    /* some data for glob */
    GPatternSpec *glob_handle;

/* private data */

    /* prepared conditions */
    GPtrArray *conditions;

    /* original search string */
    gchar *original;
    guint original_len;

    /* error code after search */
    mc_search_error_t error;
    gchar *error_str;

} mc_search_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

mc_search_t *mc_search_new (const gchar * original, guint original_len);

void mc_search_free (mc_search_t * mc_search);

#endif
