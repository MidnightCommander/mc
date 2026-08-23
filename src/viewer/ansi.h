/** \file ansi.h
 *  \brief Header: ANSI SGR escape sequence parser for mcview
 */

#ifndef MC__VIEWER_ANSI_H
#define MC__VIEWER_ANSI_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/** Maximum number of CSI parameters we track */
#define MCVIEW_ANSI_MAX_PARAMS 16

/** Default (unset) color value */
#define MCVIEW_ANSI_COLOR_DEFAULT (-1)

/*** enums ***************************************************************************************/

/** Result of feeding one byte to the ANSI parser */
typedef enum
{
    ANSI_RESULT_CHAR,    /**< regular character — render with current color */
    ANSI_RESULT_CONSUMED /**< escape sequence byte — skip, don't render */
} mcview_ansi_result_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/** ANSI SGR parser state */
typedef struct
{
    /* --- public color state (read by renderer) --- */
    int fg; /**< foreground: 0-7 base, 8-15 bright, -1 default */
    int bg; /**< background: 0-7 base, 8-15 bright, -1 default */
    gboolean bold;
    gboolean italic;
    gboolean underline;
    gboolean blink;
    gboolean reverse;

    /* --- internal parser state --- */
    gboolean in_escape; /**< seen ESC, waiting for '[' */
    gboolean in_csi;    /**< inside CSI sequence (ESC[...) */
    int params[MCVIEW_ANSI_MAX_PARAMS];
    gboolean is_colon_sep[MCVIEW_ANSI_MAX_PARAMS]; /**< TRUE if preceded by ':' */
    int param_count;
    int current_param;          /**< parameter being accumulated */
    gboolean has_current_param; /**< whether current_param has digits */
    gboolean next_is_colon;     /**< next param preceded by ':' */
} mcview_ansi_state_t;

/*** declarations of public functions ************************************************************/

/** Initialize parser state to defaults (no color, not in escape) */
void mcview_ansi_state_init (mcview_ansi_state_t *state);

/** Feed one byte to the parser.
 *  Returns ANSI_RESULT_CHAR if the byte is a displayable character.
 *  Returns ANSI_RESULT_CONSUMED if the byte is part of an escape sequence. */
mcview_ansi_result_t mcview_ansi_parse_char (mcview_ansi_state_t *state, int ch);

/*** inline functions ****************************************************************************/

#endif /* MC__VIEWER_ANSI_H */
