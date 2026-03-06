/*
   Archive browser panel plugin — shared types and constants.

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

#ifndef ARCMC_TYPES_H
#define ARCMC_TYPES_H

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

/*** typedefs (not langstruc strstruc)
 * *************************************************************/

typedef struct
{
    char *full_path; /* full path inside the archive */
    char *name;      /* basename for display */
    mode_t mode;     /* S_IFDIR | perms  or  S_IFREG | perms */
    off_t size;
    time_t mtime;
    gboolean is_virtual_dir; /* TRUE if synthesized from path components */
} arcmc_entry_t;

/* Format indices for arcmc_pack_opts_t.format */
enum
{
    ARCMC_FMT_ZIP = 0,
    ARCMC_FMT_7Z = 1,
    ARCMC_FMT_TAR_GZ = 2,
    ARCMC_FMT_TAR_BZ2 = 3,
    ARCMC_FMT_TAR_XZ = 4,
    ARCMC_FMT_TAR = 5,
    ARCMC_FMT_CPIO = 6,
    ARCMC_FMT_OTHER_COUNT = 5 /* number of "Other" formats (tar.gz .. cpio) */
};

#define ARCMC_FMT_COUNT (ARCMC_FMT_CPIO + 1)

typedef struct
{
    char *archive_path;
    int format;      /* ARCMC_FMT_* */
    int compression; /* 0=store, 1=fastest, 2=normal, 3=maximum */
    char *password;
    gboolean encrypt_files;
    gboolean encrypt_header;
    gboolean store_paths;
    gboolean delete_after;
} arcmc_pack_opts_t;

/* Saved state for nested archive browsing (stack frame). */
typedef struct arcmc_nest_frame_t
{
    struct arcmc_nest_frame_t *prev;
    char *archive_path; /* path to the outer archive on disk */
    char *current_dir;  /* dir inside the outer archive where we were */
    char *password;
    char *extfs_helper;
    GPtrArray *all_entries;
    char *temp_file; /* extracted temp file (to unlink on pop), or NULL */
} arcmc_nest_frame_t;

typedef struct
{
    mc_panel_host_t *host;
    char *archive_path;             /* path to the archive on disk */
    char *current_dir;              /* current directory inside the archive ("" = root) */
    char *password;                 /* password (if encrypted), NULL otherwise */
    GPtrArray *all_entries;         /* flat list of all entries from the archive */
    char *title_buf;                /* buffer for get_title */
    char *extfs_helper;             /* full path to extfs helper, or NULL for libarchive mode */
    arcmc_nest_frame_t *nest_stack; /* stack of outer archives for nested browsing */
} arcmc_data_t;

/* Progress dialog context for pack/extract operations */
typedef struct
{
    WDialog *dlg;
    WHLine *hline_top;      /* top separator — shows "{32%} title" */
    WLabel *lbl_archive;    /* archive path */
    WLabel *lbl_file;       /* current file path */
    WLabel *lbl_file_size;  /* "184 КБ / 1.06 МБ" */
    WGauge *gauge_file;     /* per-file progress */
    WLabel *lbl_total_size; /* "182 МБ / 559 МБ @ 17 МБ/с -00:00:22" */
    WLabel *lbl_ratio;      /* "182 МБ → 9 МБ = 5%" */
    WGauge *gauge_total;    /* total progress */

    /* tracking state */
    char *op_title;      /* operation title (e.g. "Создание архива...") */
    off_t total_bytes;   /* total bytes to process */
    off_t done_bytes;    /* bytes processed so far */
    off_t written_bytes; /* compressed bytes written */
    off_t file_bytes;    /* current file total size */
    off_t file_done;     /* current file bytes done */
    gint64 start_time;   /* g_get_monotonic_time() at start */
    int gauge_cols;      /* visual bar length in characters */
    int last_total_col;  /* last drawn column of total gauge */
    int last_file_col;   /* last drawn column of file gauge */
    gboolean aborted;    /* user pressed Abort/Esc */
    gboolean visible;    /* dlg_init() called */
} arcmc_progress_t;

/* Extension → extfs helper mapping entry */
typedef struct
{
    const char *ext;
    const char *helper;
} arcmc_extfs_map_t;

/* Extension → extfs helper mapping for formats not handled by libarchive */
extern const arcmc_extfs_map_t extfs_map[];
extern const size_t extfs_map_count;

/*** declarations (functions)
 * **********************************************************************/

void arcmc_entry_free (gpointer p);

#endif /* ARCMC_TYPES_H */
