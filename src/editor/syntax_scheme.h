/** \file
 *  \brief Header: Sublime Text color schemes for editor syntax scopes
 */

#ifndef MC__EDIT_SYNTAX_SCHEME_H
#define MC__EDIT_SYNTAX_SCHEME_H

/*** typedefs(not structures) and defined constants **********************************************/

#define SYNTAX_SCHEME_DIR "schemes"
#define SYNTAX_SCHEME_EXT ".sublime-color-scheme"

/*** enums ***************************************************************************************/

typedef enum
{
    SYNTAX_SCHEME_DEPTH_16 = 0,
    SYNTAX_SCHEME_DEPTH_256,
    SYNTAX_SCHEME_DEPTH_TRUECOLOR
} syntax_scheme_depth_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct syntax_scheme_t syntax_scheme_t;

/* resolved style: empty string means "not set" */
typedef struct
{
    char fg[16];
    char bg[16];
    char attrs[32];
} syntax_scheme_style_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean syntax_scheme_is_scope_token (const char *token);
char *syntax_scheme_token_to_scope (const char *token);

syntax_scheme_t *syntax_scheme_parse (const char *text, GError **error);
syntax_scheme_t *syntax_scheme_builtin (void);
void syntax_scheme_free (syntax_scheme_t *scheme);

void syntax_scheme_resolve (const syntax_scheme_t *scheme, const char *ctx_scope, const char *scope,
                            syntax_scheme_depth_t depth, syntax_scheme_style_t *style);

/* editor glue: resolve scope tokens with the configured scheme */
void syntax_scheme_lookup (const char *ctx_token, const char *token, syntax_scheme_style_t *style);
GPtrArray *syntax_scheme_list (void);
void syntax_scheme_done (void);

/*** inline functions ****************************************************************************/

#endif
