/*
   Archive browser panel plugin — core plugin API and actions.

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

#include <config.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

#include "arcmc-types.h"
#include "progress.h"
#include "archive-io.h"
#include "dialog-pack.h"

/*** forward declarations (file scope functions) *************************************************/

static void *arcmc_open (mc_panel_host_t *host, const char *open_path);
static void arcmc_close (void *plugin_data);
static mc_pp_result_t arcmc_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t arcmc_chdir (void *plugin_data, const char *path);
static mc_pp_result_t arcmc_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t arcmc_get_local_copy (void *plugin_data, const char *fname,
                                            char **local_path);
static mc_pp_result_t arcmc_put_file (void *plugin_data, const char *local_path,
                                      const char *dest_name);
static mc_pp_result_t arcmc_delete_items (void *plugin_data, const char **names, int count);
static const char *arcmc_get_title (void *plugin_data);
static mc_pp_result_t arcmc_handle_key (void *plugin_data, int key);
static void *arcmc_action_browse (mc_panel_host_t *host, const char *open_path);
static void *arcmc_action_create (mc_panel_host_t *host, const char *open_path);
static void *arcmc_action_extract (mc_panel_host_t *host, const char *open_path);
static void *arcmc_action_test (mc_panel_host_t *host, const char *open_path);

/*** file scope variables ************************************************************************/

static const mc_pp_action_t arcmc_actions[] = {
    { N_ ("Open archive"), arcmc_action_browse },
    { N_ ("Create archive"), arcmc_action_create },
    { N_ ("Extract archive(s)"), arcmc_action_extract },
    { N_ ("Test archive(s)"), arcmc_action_test },
};

static const mc_pp_cmd_menu_entry_t arcmc_cmd_menu[] = {
    { N_ ("Cre&ate archive"), 1, "S-F1", KEY_F (11) },
};

static const mc_panel_plugin_t arcmc_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "arcmc",
    .display_name = "Arcmc plugin",
    .proto = "Arcmc",
    .prefix = NULL,
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_CUSTOM_TITLE | MC_PPF_SHOW_IN_MENU,

    .open = arcmc_open,
    .close = arcmc_close,
    .get_items = arcmc_get_items,

    .chdir = arcmc_chdir,
    .enter = arcmc_enter,
    .view = NULL,
    .get_local_copy = arcmc_get_local_copy,
    .put_file = arcmc_put_file,
    .save_file = NULL,
    .delete_items = arcmc_delete_items,
    .get_title = arcmc_get_title,
    .handle_key = arcmc_handle_key,

    .actions = arcmc_actions,
    .action_count = G_N_ELEMENTS (arcmc_actions),
    .cmd_menu_entries = arcmc_cmd_menu,
    .cmd_menu_entry_count = G_N_ELEMENTS (arcmc_cmd_menu),
};

/*** file scope functions ************************************************************************/

static gboolean
arcmc_is_supported_archive (const char *filename)
{
    static const char *const exts[] = {
        ".tar.gz", ".tgz",      ".tar.bz2", ".tbz2", ".tar.xz", ".txz", ".tar.zst", ".tzst",
        ".tar.lz", ".tar.lzma", ".tlz",     ".tar",  ".zip",    ".7z",  ".cpio",
    };

    size_t flen, i;

    if (filename == NULL)
        return FALSE;

    flen = strlen (filename);

    for (i = 0; i < G_N_ELEMENTS (exts); i++)
    {
        size_t elen = strlen (exts[i]);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, exts[i]) == 0)
            return TRUE;
    }

    /* also accept extensions handled by extfs helpers */
    for (i = 0; i < extfs_map_count; i++)
    {
        size_t elen = strlen (extfs_map[i].ext);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, extfs_map[i].ext) == 0)
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
arcmc_detect_format (const char *filename)
{
    static const struct
    {
        const char *ext;
        const char *fmt;
    } map[] = {
        { ".tar.gz", "tar.gz" },     { ".tgz", "tar.gz" },    { ".tar.bz2", "tar.bz2" },
        { ".tbz2", "tar.bz2" },      { ".tar.xz", "tar.xz" }, { ".txz", "tar.xz" },
        { ".tar.zst", "tar.zst" },   { ".tzst", "tar.zst" },  { ".tar.lz", "tar.lz" },
        { ".tar.lzma", "tar.lzma" }, { ".tlz", "tar.lzma" },  { ".tar", "tar" },
        { ".zip", "zip" },           { ".7z", "7z" },         { ".cpio", "cpio" },
    };

    size_t i;

    for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
        size_t elen = strlen (map[i].ext);
        size_t flen = strlen (filename);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, map[i].ext) == 0)
            return map[i].fmt;
    }

    /* check extfs extensions */
    {
        size_t flen = strlen (filename);

        for (i = 0; i < extfs_map_count; i++)
        {
            size_t elen = strlen (extfs_map[i].ext);

            if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, extfs_map[i].ext) == 0)
                return extfs_map[i].ext + 1; /* skip the leading dot */
        }
    }

    return "arc";
}

/* --------------------------------------------------------------------------------------------- */

/* Collect the list of files to pack from the panel via host API. */
static GPtrArray *
arcmc_collect_files (mc_panel_host_t *host)
{
    GPtrArray *files;
    int marked;

    files = g_ptr_array_new_with_free_func (g_free);
    marked = host->get_marked_count (host);

    if (marked > 0)
    {
        int idx = 0;
        const GString *gs;

        while ((gs = host->get_next_marked (host, &idx)) != NULL)
        {
            g_ptr_array_add (files, g_strdup (gs->str));
            idx++;
        }
    }
    else
    {
        const GString *gs;

        gs = host->get_current (host);
        if (gs != NULL && gs->str != NULL && gs->str[0] != '\0')
            g_ptr_array_add (files, g_strdup (gs->str));
    }

    return files;
}

/* --------------------------------------------------------------------------------------------- */

/* Build default archive name based on panel selection.
   - single file/dir under cursor (no marks): use its name
   - one marked file/dir: use its name
   - multiple marked: use current directory basename
   Appends the default extension (.zip). */
static char *
arcmc_build_default_archive_name (mc_panel_host_t *host, const char *open_path)
{
    const char *base_name = NULL;
    const GString *gs;
    int marked;
    char *result;

    marked = host->get_marked_count (host);

    if (marked <= 1)
    {
        if (marked == 1)
        {
            int idx = 0;

            gs = host->get_next_marked (host, &idx);
        }
        else
            gs = host->get_current (host);

        if (gs != NULL && gs->str != NULL && gs->str[0] != '\0')
            base_name = gs->str;
    }

    if (base_name == NULL)
    {
        /* multiple marked or no current — use directory basename */
        const char *slash;

        slash = strrchr (open_path, '/');
        base_name = (slash != NULL && slash[1] != '\0') ? slash + 1 : open_path;
    }

    /* strip existing known extension if present */
    {
        size_t i;
        size_t name_len = strlen (base_name);

        for (i = 0; i < G_N_ELEMENTS (format_extensions); i++)
        {
            size_t ext_len = strlen (format_extensions[i]);

            if (name_len > ext_len
                && g_ascii_strcasecmp (base_name + name_len - ext_len, format_extensions[i]) == 0)
            {
                char *stripped = g_strndup (base_name, name_len - ext_len);

                result = g_strconcat (stripped, format_extensions[0], NULL);
                g_free (stripped);
                return result;
            }
        }
    }

    result = g_strconcat (base_name, format_extensions[0], NULL);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
arcmc_open (mc_panel_host_t *host, const char *open_path)
{
    return arcmc_action_browse (host, open_path);
}

/* --------------------------------------------------------------------------------------------- */

static void *
arcmc_action_browse (mc_panel_host_t *host, const char *open_path)
{
    arcmc_data_t *data;
    char *archive_file;

    /* If open_path points to a supported archive file, use it directly */
    if (open_path != NULL && g_file_test (open_path, G_FILE_TEST_IS_REGULAR)
        && (arcmc_is_supported_archive (open_path)
#ifdef HAVE_LIBMAGIC
            || arcmc_is_archive_by_content (open_path)
#endif
                ))
    {
        archive_file = g_strdup (open_path);
    }
    else if (open_path != NULL && g_file_test (open_path, G_FILE_TEST_IS_REGULAR))
    {
        /* file exists but not recognized as archive — silently refuse */
        return NULL;
    }
    else if (open_path == NULL || !g_file_test (open_path, G_FILE_TEST_IS_REGULAR))
    {
        /* no path or not a file — show dialog (called from menu) */
        archive_file = input_expand_dialog (_ ("Archive browser"), _ ("Archive file:"),
                                            "arcmc-archive-path", "", INPUT_COMPLETE_FILENAMES);
        if (archive_file == NULL || archive_file[0] == '\0')
        {
            g_free (archive_file);
            return NULL;
        }

        if (!g_file_test (archive_file, G_FILE_TEST_IS_REGULAR))
        {
            message (D_ERROR, MSG_ERROR, _ ("File not found: %s"), archive_file);
            g_free (archive_file);
            return NULL;
        }
    }
    else
    {
        return NULL;
    }

    data = g_new0 (arcmc_data_t, 1);
    data->host = host;
    data->archive_path = archive_file;
    data->current_dir = g_strdup ("");
    data->password = NULL;
    data->all_entries = NULL;
    data->title_buf = NULL;
    data->extfs_helper = NULL;
    data->nest_stack = NULL;

    if (!arcmc_try_open (data))
    {
        if (data->all_entries != NULL)
            g_ptr_array_free (data->all_entries, TRUE);
        g_free (data->archive_path);
        g_free (data->current_dir);
        g_free (data->password);
        g_free (data->extfs_helper);
        g_free (data);
        return NULL;
    }

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void *
arcmc_action_create (mc_panel_host_t *host, const char *open_path)
{
    arcmc_pack_opts_t pack_opts;
    char *default_name;

    default_name = arcmc_build_default_archive_name (host, open_path);

    memset (&pack_opts, 0, sizeof (pack_opts));
    if (arcmc_show_pack_dialog (&pack_opts, default_name))
    {
        GPtrArray *files;

        files = arcmc_collect_files (host);

        if (files->len > 0)
        {
            gboolean ok;

            ok = arcmc_do_pack (&pack_opts, open_path, files);

            if (ok)
            {
                const char *bn;

                bn = strrchr (pack_opts.archive_path, '/');
                host->focus_after = g_strdup (bn != NULL ? bn + 1 : pack_opts.archive_path);
            }
            else
                host->message (host, D_ERROR, MSG_ERROR, _ ("Failed to create archive"));
        }

        g_ptr_array_free (files, TRUE);
        g_free (pack_opts.archive_path);
        g_free (pack_opts.password);
    }

    g_free (default_name);
    return NULL; /* standalone dialog, no panel activation */
}

/* --------------------------------------------------------------------------------------------- */

static void *
arcmc_action_extract (mc_panel_host_t *host, const char *open_path)
{
    (void) open_path;

    host->message (host, D_NORMAL, _ ("Extract"), _ ("Extract: not yet implemented"));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void *
arcmc_action_test (mc_panel_host_t *host, const char *open_path)
{
    (void) open_path;

    host->message (host, D_NORMAL, _ ("Test"), _ ("Test archive: not yet implemented"));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_close (void *plugin_data)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;

    /* free nest stack (if user closes while inside nested archives) */
    while (data->nest_stack != NULL)
    {
        arcmc_nest_frame_t *f = data->nest_stack;

        data->nest_stack = f->prev;
        if (f->all_entries != NULL)
            g_ptr_array_free (f->all_entries, TRUE);
        g_free (f->archive_path);
        g_free (f->current_dir);
        g_free (f->password);
        g_free (f->extfs_helper);
        if (f->temp_file != NULL)
        {
            unlink (f->temp_file);
            g_free (f->temp_file);
        }
        g_free (f);
    }

    if (data->all_entries != NULL)
        g_ptr_array_free (data->all_entries, TRUE);
    g_free (data->archive_path);
    g_free (data->current_dir);
    g_free (data->password);
    g_free (data->title_buf);
    g_free (data->extfs_helper);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_get_items (void *plugin_data, void *list_ptr)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;
    guint i;

    if (data->all_entries == NULL)
        return MC_PPR_OK;

    for (i = 0; i < data->all_entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (data->all_entries, i);
        const char *child_name;

        child_name = is_direct_child (e->full_path, data->current_dir);
        if (child_name != NULL)
            mc_pp_add_entry (list_ptr, child_name, e->mode, e->size, e->mtime);
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_chdir (void *plugin_data, const char *path)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        if (data->current_dir[0] == '\0')
        {
            /* if we're inside a nested archive, pop back to the outer one */
            if (data->nest_stack != NULL)
            {
                arcmc_nest_frame_t *f = data->nest_stack;

                /* clean up current (nested) state */
                if (data->all_entries != NULL)
                    g_ptr_array_free (data->all_entries, TRUE);
                g_free (data->archive_path);
                g_free (data->current_dir);
                g_free (data->password);
                g_free (data->extfs_helper);

                /* restore outer state */
                data->archive_path = f->archive_path;
                data->current_dir = f->current_dir;
                data->password = f->password;
                data->extfs_helper = f->extfs_helper;
                data->all_entries = f->all_entries;
                data->nest_stack = f->prev;

                /* remove the temp file */
                if (f->temp_file != NULL)
                {
                    unlink (f->temp_file);
                    g_free (f->temp_file);
                }

                g_free (f);
                return MC_PPR_OK;
            }

            {
                const char *bn;

                bn = strrchr (data->archive_path, '/');
                data->host->focus_after = g_strdup (bn != NULL ? bn + 1 : data->archive_path);
                return MC_PPR_CLOSE;
            }
        }

        {
            char *parent;

            parent = get_parent_dir (data->current_dir);
            g_free (data->current_dir);
            data->current_dir = parent;
        }

        return MC_PPR_OK;
    }

    /* verify target directory exists */
    {
        char *new_dir;
        guint i;
        gboolean found = FALSE;

        new_dir = build_child_path (data->current_dir, path);

        for (i = 0; i < data->all_entries->len; i++)
        {
            const arcmc_entry_t *e =
                (const arcmc_entry_t *) g_ptr_array_index (data->all_entries, i);

            if (strcmp (e->full_path, new_dir) == 0 && S_ISDIR (e->mode))
            {
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            g_free (new_dir);
            return MC_PPR_FAILED;
        }

        g_free (data->current_dir);
        data->current_dir = new_dir;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_enter (void *plugin_data, const char *name, const struct stat *st)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;

    if (st != NULL && S_ISDIR (st->st_mode))
        return MC_PPR_NOT_SUPPORTED; /* let mc call chdir */

    /* if the file has a known archive extension, open it as nested */
    if (arcmc_is_supported_archive (name))
    {
        char *local_path = NULL;
        mc_pp_result_t r;

        r = arcmc_extract_to_temp (data, name, &local_path);

        if (r != MC_PPR_OK || local_path == NULL)
        {
            g_free (local_path);
            return MC_PPR_FAILED;
        }

        r = arcmc_push_nested (data, local_path);
        if (r != MC_PPR_OK)
        {
            message (D_ERROR, MSG_ERROR, _ ("Cannot open nested archive"));
            return MC_PPR_FAILED;
        }

        return MC_PPR_OK;
    }

    /* for unknown extensions, extract and check content with libmagic */
#ifdef HAVE_LIBMAGIC
    if (st != NULL && S_ISREG (st->st_mode))
    {
        char *local_path = NULL;
        mc_pp_result_t r;

        r = arcmc_extract_to_temp (data, name, &local_path);

        if (r != MC_PPR_OK || local_path == NULL)
        {
            g_free (local_path);
            return MC_PPR_NOT_SUPPORTED;
        }

        if (arcmc_is_archive_by_content (local_path))
        {
            r = arcmc_push_nested (data, local_path);
            if (r == MC_PPR_OK)
                return MC_PPR_OK;
            /* push failed — fall through to NOT_SUPPORTED */
        }
        else
        {
            unlink (local_path);
            g_free (local_path);
        }
    }
#endif /* HAVE_LIBMAGIC */

    /* for files, let mc use get_local_copy + viewer */
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;
    char *target_path;
    mc_pp_result_t result;
    arcmc_progress_t *progress;
    off_t file_size = 0;
    guint i;

    target_path = build_child_path (data->current_dir, fname);

    /* extfs mode — use helper's copyout */
    if (data->extfs_helper != NULL)
    {
        result = arcmc_extract_entry_extfs (data, target_path, local_path);
        g_free (target_path);
        return result;
    }

    /* find file size for progress */
    if (data->all_entries != NULL)
    {
        for (i = 0; i < data->all_entries->len; i++)
        {
            const arcmc_entry_t *e =
                (const arcmc_entry_t *) g_ptr_array_index (data->all_entries, i);

            if (strcmp (e->full_path, target_path) == 0)
            {
                file_size = e->size;
                break;
            }
        }
    }

    progress = arcmc_progress_create (_ ("Extracting..."), data->archive_path, file_size);
    result = arcmc_extract_entry (data, target_path, local_path, progress);

    /* If extraction failed and no password set, try asking for one */
    while (result != MC_PPR_OK && data->password == NULL && !progress->aborted)
    {
        char *pw;

        pw = input_dialog (_ ("Archive password"), _ ("Enter password:"), "arcmc-password",
                           INPUT_PASSWORD, INPUT_COMPLETE_NONE);
        if (pw == NULL)
            break;

        data->password = pw;
        progress->done_bytes = 0;
        progress->written_bytes = 0;
        result = arcmc_extract_entry (data, target_path, local_path, progress);
    }

    arcmc_progress_destroy (progress);
    g_free (target_path);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_put_file (void *plugin_data, const char *local_path, const char *dest_name)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;
    char *archive_name;
    arcmc_progress_t *progress;
    off_t total_size;
    struct stat st;
    gboolean ok;

    if (data->current_dir[0] != '\0')
        archive_name = g_strconcat (data->current_dir, "/", dest_name, NULL);
    else
        archive_name = g_strdup (dest_name);

    /* extfs mode — use helper's copyin */
    if (data->extfs_helper != NULL)
    {
        ok = arcmc_extfs_run_cmd (data->extfs_helper, " copyin ", data->archive_path, archive_name,
                                  local_path);
        g_free (archive_name);

        if (!ok)
        {
            message (D_ERROR, MSG_ERROR, _ ("extfs helper does not support adding files"));
            return MC_PPR_FAILED;
        }

        arcmc_read_archive_extfs (data);
        return MC_PPR_OK;
    }

    /* total = existing entries + new file */
    total_size = arcmc_entries_total_size (data->all_entries);
    if (lstat (local_path, &st) == 0)
        total_size += st.st_size;

    progress = arcmc_progress_create (_ ("Adding to archive..."), data->archive_path, total_size);

    ok = arcmc_archive_add_file (data->archive_path, local_path, archive_name, data->password,
                                 progress);

    arcmc_progress_destroy (progress);

    if (!ok)
    {
        g_free (archive_name);
        return MC_PPR_FAILED;
    }

    g_free (archive_name);

    /* re-read archive to update the entry list */
    arcmc_read_archive (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_delete_items (void *plugin_data, const char **names, int count)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;
    char **full_paths;
    int i;
    gboolean ok;
    arcmc_progress_t *progress;
    off_t total_size;

    full_paths = g_new (char *, count);

    for (i = 0; i < count; i++)
    {
        if (data->current_dir[0] != '\0')
            full_paths[i] = g_strconcat (data->current_dir, "/", names[i], NULL);
        else
            full_paths[i] = g_strdup (names[i]);
    }

    /* extfs mode — use helper's rm command per item */
    if (data->extfs_helper != NULL)
    {
        gboolean any_failed = FALSE;

        for (i = 0; i < count; i++)
        {
            if (!arcmc_extfs_run_cmd (data->extfs_helper, " rm ", data->archive_path, full_paths[i],
                                      NULL))
                any_failed = TRUE;
        }

        for (i = 0; i < count; i++)
            g_free (full_paths[i]);
        g_free (full_paths);

        if (any_failed)
        {
            message (D_ERROR, MSG_ERROR, _ ("extfs helper does not support deleting files"));
            return MC_PPR_FAILED;
        }

        arcmc_read_archive_extfs (data);
        return MC_PPR_OK;
    }

    /* total = entries that will be kept (not deleted) */
    total_size = arcmc_entries_total_size (data->all_entries);

    progress =
        arcmc_progress_create (_ ("Deleting from archive..."), data->archive_path, total_size);

    ok = arcmc_archive_delete (data->archive_path, (const char **) full_paths, count,
                               data->password, progress);

    arcmc_progress_destroy (progress);

    for (i = 0; i < count; i++)
        g_free (full_paths[i]);
    g_free (full_paths);

    if (!ok)
        return MC_PPR_FAILED;

    arcmc_read_archive (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
arcmc_get_title (void *plugin_data)
{
    arcmc_data_t *data = (arcmc_data_t *) plugin_data;
    const char *basename_ptr;

    g_free (data->title_buf);

    basename_ptr = strrchr (data->archive_path, '/');
    if (basename_ptr != NULL)
        basename_ptr++;
    else
        basename_ptr = data->archive_path;

    if (data->current_dir[0] == '\0')
        data->title_buf =
            g_strdup_printf ("%s:%s", arcmc_detect_format (basename_ptr), basename_ptr);
    else
        data->title_buf = g_strdup_printf ("%s:%s:/%s", arcmc_detect_format (basename_ptr),
                                           basename_ptr, data->current_dir);

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
arcmc_handle_key (void *plugin_data, int key)
{
    arcmc_pack_opts_t opts;

    (void) plugin_data;

    /* Shift+F1 or Shift+F5 */
    if (key != KEY_F (11) && key != KEY_F (15))
        return MC_PPR_NOT_SUPPORTED;

    memset (&opts, 0, sizeof (opts));

    if (arcmc_show_pack_dialog (&opts, ""))
    {
        /* TODO: implement packing logic */
        g_free (opts.archive_path);
        g_free (opts.password);
    }

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Entry point called by the panel plugin loader */
const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &arcmc_plugin;
}

/* --------------------------------------------------------------------------------------------- */
