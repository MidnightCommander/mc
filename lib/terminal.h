/** \file terminal.h
 *  \brief Header: terminal emulation logic.
 */

#ifndef MC_TERMINAL_H
#define MC_TERMINAL_H

#include <sys/types.h>
#include <inttypes.h>  // uint32_t

#include "lib/global.h"  // include <glib.h>

struct csi_command_t
{
    char private_mode;
    uint32_t params[16][4];
    size_t param_count;
};
gboolean parse_csi (struct csi_command_t *out, const char **sptr, const char *end);

char *strip_ctrl_codes (char *s);

/* Replaces "\\E" and "\\e" with "\033". Replaces "^" + [a-z] with
 * ((char) 1 + (c - 'a')). The same goes for "^" + [A-Z].
 * Returns a newly allocated string. */
char *convert_controls (const char *s);

#endif
