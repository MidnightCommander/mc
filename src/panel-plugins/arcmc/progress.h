/*
   Archive browser panel plugin — progress dialog.

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

#ifndef ARCMC_PROGRESS_H
#define ARCMC_PROGRESS_H

#include "arcmc-types.h"

/*** declarations (functions)
 * **********************************************************************/

arcmc_progress_t *arcmc_progress_create (const char *title, const char *archive_path,
                                         off_t total_bytes);
void arcmc_progress_destroy (arcmc_progress_t *p);
gboolean arcmc_progress_update (arcmc_progress_t *p, const char *filename, off_t file_size,
                                off_t file_done, off_t total_done, off_t written);

#endif /* ARCMC_PROGRESS_H */
