/** \file terminal.h
 *  \brief Header: terminal emulation logic.
 */

#ifndef MC_TERMINAL_H
#define MC_TERMINAL_H

#include <sys/types.h>
#include <inttypes.h>  // uint32_t

#include "lib/global.h"  // include <glib.h>

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    char private_mode;
    uint32_t params[16][4];
    size_t param_count;
} csi_command_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean parse_csi (csi_command_t *out, const char **sptr, const char *end);

char *strip_ctrl_codes (char *s);

char *encode_controls (const char *s, ssize_t len);
GString *decode_controls (const char *s);

/*** inline functions ****************************************************************************/

#endif
