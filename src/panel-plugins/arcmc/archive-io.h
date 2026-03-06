/*
   Archive browser panel plugin — archive read/write/extract and extfs helpers.

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

#ifndef ARCMC_ARCHIVE_IO_H
#define ARCMC_ARCHIVE_IO_H

#include "arcmc-types.h"

/*** declarations (functions)
 * **********************************************************************/

/* Path utilities */
char *get_parent_dir (const char *current_dir);
char *build_child_path (const char *current_dir, const char *name);
const char *is_direct_child (const char *entry_path, const char *dir);

/* Reading */
char *arcmc_find_extfs_helper (const char *archive_path);
gboolean arcmc_read_archive (arcmc_data_t *data);
gboolean arcmc_read_archive_extfs (arcmc_data_t *data);
gboolean arcmc_try_open (arcmc_data_t *data);

#ifdef HAVE_LIBMAGIC
gboolean arcmc_is_archive_by_content (const char *path);
#endif

/* Writing */
off_t arcmc_entries_total_size (GPtrArray *all_entries);
gboolean arcmc_archive_add_file (const char *archive_path, const char *local_path,
                                 const char *archive_name, const char *password,
                                 arcmc_progress_t *p);
gboolean arcmc_archive_delete (const char *archive_path, const char **del_paths, int del_count,
                               const char *password, arcmc_progress_t *p);
gboolean arcmc_do_pack (const arcmc_pack_opts_t *opts, const char *cwd, GPtrArray *files);

/* Extraction */
mc_pp_result_t arcmc_extract_entry (arcmc_data_t *data, const char *target_path, char **local_path,
                                    arcmc_progress_t *p);
mc_pp_result_t arcmc_extract_entry_extfs (arcmc_data_t *data, const char *target_path,
                                          char **local_path);
gboolean arcmc_extfs_run_cmd (const char *helper, const char *cmd_name, const char *archive_path,
                              const char *stored_name, const char *local_name);
mc_pp_result_t arcmc_extract_to_temp (arcmc_data_t *data, const char *name, char **local_path);
mc_pp_result_t arcmc_push_nested (arcmc_data_t *data, char *local_path);

#endif /* ARCMC_ARCHIVE_IO_H */
