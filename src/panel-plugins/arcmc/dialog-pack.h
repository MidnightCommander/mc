/*
   Archive browser panel plugin — pack dialog UI.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ARCMC_DIALOG_PACK_H
#define ARCMC_DIALOG_PACK_H

#include "arcmc-types.h"

/*** declarations (variables) *********************************************************************/

extern const char *const format_extensions[ARCMC_FMT_COUNT];

/*** declarations (functions)
 * **********************************************************************/

gboolean arcmc_show_pack_dialog (arcmc_pack_opts_t *opts, const char *initial_path);

#endif /* ARCMC_DIALOG_PACK_H */
