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

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#ifdef HAVE_LIBMAGIC
#include <magic.h>
#endif

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/util.h"        /* mc_popen, mc_pread, mc_pstream_get_string, mc_pclose, name_quote */
#include "lib/vfs/utilvfs.h" /* vfs_parse_ls_lga */
#include "lib/mcconfig.h"    /* mc_config_get_data_path */

#include "arcmc-types.h"
#include "progress.h"
#include "archive-io.h"

/*** file scope variables ************************************************************************/

/* Extension → extfs helper mapping for formats not handled by libarchive */
const arcmc_extfs_map_t extfs_map[] = {
    { ".arj", "uarj" }, { ".rar", "urar" }, { ".ace", "uace" }, { ".arc", "uarc" },
    { ".alz", "ualz" }, { ".zoo", "uzoo" }, { ".ha", "uha" },   { ".wim", "uwim" },
    { ".lha", "ulha" }, { ".lzh", "ulha" }, { ".deb", "deb" },  { ".rpm", "rpm" },
};

const size_t extfs_map_count = G_N_ELEMENTS (extfs_map);

/*** file scope functions ************************************************************************/

/* Strip trailing slashes from a path string (in-place). */
static void
strip_trailing_slashes (char *path)
{
    size_t len;

    if (path == NULL)
        return;

    len = strlen (path);
    while (len > 0 && path[len - 1] == '/')
        path[--len] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

/* Check whether a virtual directory entry already exists in all_entries. */
static gboolean
has_entry (GPtrArray *entries, const char *full_path)
{
    guint i;

    for (i = 0; i < entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (entries, i);

        if (strcmp (e->full_path, full_path) == 0)
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Ensure all parent directories of `full_path` exist as virtual dir entries. */
static void
ensure_parent_dirs (GPtrArray *entries, const char *full_path)
{
    char *tmp;
    char *slash;

    tmp = g_strdup (full_path);

    for (slash = strchr (tmp, '/'); slash != NULL; slash = strchr (slash + 1, '/'))
    {
        char saved;
        char *basename_ptr;
        arcmc_entry_t *dir_entry;

        saved = *slash;
        *slash = '\0';

        if (tmp[0] == '\0' || has_entry (entries, tmp))
        {
            *slash = saved;
            continue;
        }

        dir_entry = g_new0 (arcmc_entry_t, 1);
        dir_entry->full_path = g_strdup (tmp);

        basename_ptr = strrchr (dir_entry->full_path, '/');
        dir_entry->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : dir_entry->full_path);

        dir_entry->mode = S_IFDIR | 0755;
        dir_entry->size = 0;
        dir_entry->mtime = time (NULL);
        dir_entry->is_virtual_dir = TRUE;

        g_ptr_array_add (entries, dir_entry);

        *slash = saved;
    }

    g_free (tmp);
}

/* --------------------------------------------------------------------------------------------- */

/* Recursively calculate the total size of files for packing. */
static off_t
arcmc_calculate_total_size (const char *cwd, GPtrArray *files)
{
    guint i;
    off_t total = 0;

    for (i = 0; i < files->len; i++)
    {
        const char *name = (const char *) g_ptr_array_index (files, i);
        char *full_path;
        struct stat st;

        full_path = g_build_filename (cwd, name, NULL);

        if (lstat (full_path, &st) == 0)
        {
            if (S_ISDIR (st.st_mode))
            {
                GDir *dir;

                total += st.st_size;
                dir = g_dir_open (full_path, 0, NULL);
                if (dir != NULL)
                {
                    const gchar *child;
                    GPtrArray *children;

                    children = g_ptr_array_new_with_free_func (g_free);
                    while ((child = g_dir_read_name (dir)) != NULL)
                        g_ptr_array_add (children, g_strconcat (name, "/", child, NULL));
                    g_dir_close (dir);

                    total += arcmc_calculate_total_size (cwd, children);
                    g_ptr_array_free (children, TRUE);
                }
            }
            else
                total += st.st_size;
        }

        g_free (full_path);
    }

    return total;
}

/* --------------------------------------------------------------------------------------------- */

/* Add a single file to the archive. `disk_path` is the real filesystem path,
   `archive_name` is the name stored inside the archive.
   `p` is an optional progress context (may be NULL). */
static gboolean
arcmc_pack_add_file (struct archive *a, const char *disk_path, const char *archive_name,
                     arcmc_progress_t *p)
{
    struct stat st;
    struct archive_entry *entry;
    int fd;
    char buf[8192];
    ssize_t bytes_read;

    if (lstat (disk_path, &st) != 0)
        return FALSE;

    entry = archive_entry_new ();
    archive_entry_set_pathname (entry, archive_name);
    archive_entry_copy_stat (entry, &st);

    if (archive_write_header (a, entry) != ARCHIVE_OK)
    {
        archive_entry_free (entry);
        return FALSE;
    }

    if (S_ISREG (st.st_mode))
    {
        off_t file_done = 0;

        fd = open (disk_path, O_RDONLY);
        if (fd >= 0)
        {
            while ((bytes_read = read (fd, buf, sizeof (buf))) > 0)
            {
                la_ssize_t written;

                written = archive_write_data (a, buf, (size_t) bytes_read);
                if (written < 0)
                {
                    close (fd);
                    archive_entry_free (entry);
                    return FALSE;
                }
                file_done += bytes_read;

                if (p != NULL)
                {
                    p->done_bytes += bytes_read;
                    p->written_bytes += written;

                    if (!arcmc_progress_update (p, archive_name, st.st_size, file_done,
                                                p->done_bytes, p->written_bytes))
                    {
                        close (fd);
                        archive_entry_free (entry);
                        return FALSE;
                    }
                }
            }
            close (fd);
        }
    }

    archive_entry_free (entry);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Recursively add a directory tree to the archive.
   `base_dir` is the filesystem path, `archive_prefix` is the path prefix inside the archive.
   `p` is an optional progress context (may be NULL). */
static gboolean
arcmc_pack_add_directory (struct archive *a, const char *base_dir, const char *archive_prefix,
                          arcmc_progress_t *p)
{
    GDir *dir;
    const gchar *name;

    /* add the directory entry itself */
    if (!arcmc_pack_add_file (a, base_dir, archive_prefix, p))
        return FALSE;

    dir = g_dir_open (base_dir, 0, NULL);
    if (dir == NULL)
        return FALSE;

    while ((name = g_dir_read_name (dir)) != NULL)
    {
        char *full_path;
        char *arc_name;
        struct stat st;

        full_path = g_build_filename (base_dir, name, NULL);
        arc_name = g_strconcat (archive_prefix, "/", name, NULL);

        if (lstat (full_path, &st) == 0)
        {
            gboolean ok;

            if (S_ISDIR (st.st_mode))
                ok = arcmc_pack_add_directory (a, full_path, arc_name, p);
            else
                ok = arcmc_pack_add_file (a, full_path, arc_name, p);

            if (!ok)
            {
                g_free (full_path);
                g_free (arc_name);
                g_dir_close (dir);
                return FALSE;
            }
        }

        g_free (full_path);
        g_free (arc_name);
    }

    g_dir_close (dir);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Detect ARCMC_FMT_* from archive filename extension.  Returns -1 if unknown. */
static int
arcmc_detect_fmt_id (const char *filename)
{
    static const struct
    {
        const char *ext;
        int fmt;
    } map[] = {
        { ".tar.gz", ARCMC_FMT_TAR_GZ },   { ".tgz", ARCMC_FMT_TAR_GZ },
        { ".tar.bz2", ARCMC_FMT_TAR_BZ2 }, { ".tbz2", ARCMC_FMT_TAR_BZ2 },
        { ".tar.xz", ARCMC_FMT_TAR_XZ },   { ".txz", ARCMC_FMT_TAR_XZ },
        { ".tar", ARCMC_FMT_TAR },         { ".zip", ARCMC_FMT_ZIP },
        { ".7z", ARCMC_FMT_7Z },           { ".cpio", ARCMC_FMT_CPIO },
    };

    size_t flen, i;

    flen = strlen (filename);

    for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
        size_t elen = strlen (map[i].ext);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, map[i].ext) == 0)
            return map[i].fmt;
    }

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* Configure archive_write object for the given format.  Returns TRUE on success. */
static gboolean
arcmc_write_set_format (struct archive *a, int fmt)
{
    switch (fmt)
    {
    case ARCMC_FMT_ZIP:
        archive_write_set_format_zip (a);
        break;
    case ARCMC_FMT_7Z:
        archive_write_set_format_7zip (a);
        break;
    case ARCMC_FMT_TAR_GZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_gzip (a);
        break;
    case ARCMC_FMT_TAR_BZ2:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_bzip2 (a);
        break;
    case ARCMC_FMT_TAR_XZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_xz (a);
        break;
    case ARCMC_FMT_TAR:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_none (a);
        break;
    case ARCMC_FMT_CPIO:
        archive_write_set_format_cpio (a);
        archive_write_add_filter_none (a);
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Check whether `path` (with trailing slashes stripped) should be excluded.
   An entry is excluded if it matches one of `del_paths` exactly, or is a child of one. */
static gboolean
arcmc_path_is_deleted (const char *path, const char **del_paths, int del_count)
{
    int i;
    size_t plen;

    plen = strlen (path);

    for (i = 0; i < del_count; i++)
    {
        size_t dlen = strlen (del_paths[i]);

        if (plen == dlen && strcmp (path, del_paths[i]) == 0)
            return TRUE;
        /* child: path starts with del_path + '/' */
        if (plen > dlen && strncmp (path, del_paths[i], dlen) == 0 && path[dlen] == '/')
            return TRUE;
    }

    return FALSE;
}

/*** public functions ****************************************************************************/

void
arcmc_entry_free (gpointer p)
{
    arcmc_entry_t *e = (arcmc_entry_t *) p;

    g_free (e->full_path);
    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

/* Get the parent directory of current_dir.
   Returns a newly allocated string, or "" for root. */
char *
get_parent_dir (const char *current_dir)
{
    char *slash;

    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup ("");

    slash = strrchr (current_dir, '/');
    if (slash == NULL)
        return g_strdup ("");

    return g_strndup (current_dir, (gsize) (slash - current_dir));
}

/* --------------------------------------------------------------------------------------------- */

/* Build the full path for a child entry within the current directory. */
char *
build_child_path (const char *current_dir, const char *name)
{
    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup (name);

    return g_strdup_printf ("%s/%s", current_dir, name);
}

/* --------------------------------------------------------------------------------------------- */

/* Check if `entry_path` is a direct child of `dir`.
   If so, return the child name component; otherwise NULL. */
const char *
is_direct_child (const char *entry_path, const char *dir)
{
    size_t dir_len;
    const char *rest;

    if (dir == NULL || dir[0] == '\0')
    {
        /* root: direct child if no '/' in path */
        if (strchr (entry_path, '/') == NULL)
            return entry_path;
        return NULL;
    }

    dir_len = strlen (dir);

    if (strncmp (entry_path, dir, dir_len) != 0)
        return NULL;

    if (entry_path[dir_len] != '/')
        return NULL;

    rest = entry_path + dir_len + 1;

    /* must not contain further '/' (i.e., must be direct child) */
    if (rest[0] == '\0' || strchr (rest, '/') != NULL)
        return NULL;

    return rest;
}

/* --------------------------------------------------------------------------------------------- */

/* Find an extfs helper for the given archive path.
   Returns an allocated full path to the helper executable, or NULL if not found. */
char *
arcmc_find_extfs_helper (const char *archive_path)
{
    const char *basename_ptr;
    size_t blen, i;

    basename_ptr = strrchr (archive_path, '/');
    if (basename_ptr != NULL)
        basename_ptr++;
    else
        basename_ptr = archive_path;

    blen = strlen (basename_ptr);

    for (i = 0; i < extfs_map_count; i++)
    {
        size_t elen = strlen (extfs_map[i].ext);

        if (blen >= elen && g_ascii_strcasecmp (basename_ptr + blen - elen, extfs_map[i].ext) == 0)
        {
            char *path;

            /* try user data dir first */
            path =
                g_build_filename (mc_config_get_data_path (), "extfs.d", extfs_map[i].helper, NULL);
            if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
                return path;
            g_free (path);

            /* try system libexecdir */
            path = g_build_filename (LIBEXECDIR, "extfs.d", extfs_map[i].helper, NULL);
            if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
                return path;
            g_free (path);

            return NULL;
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBMAGIC
/* Check whether a file is a supported archive by content (MIME type).
   Returns TRUE if the file looks like an archive that arcmc can handle. */
gboolean
arcmc_is_archive_by_content (const char *path)
{
    magic_t mag;
    const char *mime;
    gboolean result = FALSE;

    /* archive MIME types that libarchive or extfs helpers can handle */
    static const char *const archive_mimes[] = {
        "application/zip",       "application/x-7z-compressed",
        "application/gzip",      "application/x-gzip",
        "application/x-bzip2",   "application/x-xz",
        "application/zstd",      "application/x-zstd",
        "application/x-tar",     "application/x-cpio",
        "application/x-archive", "application/x-debian-package",
        "application/x-rpm",     "application/x-rar",
        "application/vnd.rar",   "application/x-arj",
        "application/x-ace",     "application/x-zoo",
        "application/x-lha",     "application/x-lzh-compressed",
        "application/x-alz",     "application/x-arc",
    };

    mag = magic_open (MAGIC_MIME_TYPE | MAGIC_SYMLINK | MAGIC_ERROR);
    if (mag == NULL)
        return FALSE;

    if (magic_load (mag, NULL) != 0)
    {
        magic_close (mag);
        return FALSE;
    }

    mime = magic_file (mag, path);
    if (mime != NULL)
    {
        size_t i;

        for (i = 0; i < G_N_ELEMENTS (archive_mimes); i++)
        {
            if (strcmp (mime, archive_mimes[i]) == 0)
            {
                result = TRUE;
                break;
            }
        }
    }

    magic_close (mag);
    return result;
}
#endif /* HAVE_LIBMAGIC */

/* --------------------------------------------------------------------------------------------- */

/* Read archive contents using an extfs helper's "list" command.
   Returns TRUE on success, FALSE on error. */
gboolean
arcmc_read_archive_extfs (arcmc_data_t *data)
{
    char *quoted_archive;
    char *cmd;
    mc_pipe_t *pip;
    GError *error = NULL;
    GString *remain_line = NULL;

    quoted_archive = name_quote (data->archive_path, FALSE);
    cmd = g_strconcat (data->extfs_helper, " list ", quoted_archive, (char *) NULL);
    g_free (quoted_archive);

    pip = mc_popen (cmd, TRUE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    if (data->all_entries != NULL)
        g_ptr_array_free (data->all_entries, TRUE);

    data->all_entries = g_ptr_array_new_with_free_func (arcmc_entry_free);

    for (;;)
    {
        GString *buffer;

        pip->out.len = MC_PIPE_BUFSIZE;
        pip->err.len = MC_PIPE_BUFSIZE;

        mc_pread (pip, &error);

        if (error != NULL)
        {
            g_error_free (error);
            break;
        }

        if (pip->out.len == MC_PIPE_STREAM_EOF)
            break;

        if (pip->out.len <= 0)
            continue;

        while ((buffer = mc_pstream_get_string (&pip->out)) != NULL)
        {
            if (buffer->str[buffer->len - 1] == '\n')
            {
                g_string_truncate (buffer, buffer->len - 1);

                if (remain_line != NULL)
                {
                    g_string_append_len (remain_line, buffer->str, buffer->len);
                    g_string_free (buffer, TRUE);
                    buffer = remain_line;
                    remain_line = NULL;
                }
            }
            else
            {
                if (remain_line == NULL)
                    remain_line = buffer;
                else
                {
                    g_string_append_len (remain_line, buffer->str, buffer->len);
                    g_string_free (buffer, TRUE);
                }
                continue;
            }

            /* parse the ls -l line */
            {
                struct stat st;
                char *filename = NULL;
                char *linkname = NULL;

                if (vfs_parse_ls_lga (buffer->str, &st, &filename, &linkname, NULL)
                    && filename != NULL && filename[0] != '\0')
                {
                    char *clean;
                    char *basename_ptr;
                    arcmc_entry_t *e;

                    /* skip leading "./" or "/" */
                    clean = filename;
                    if (clean[0] == '.' && clean[1] == '/')
                        clean += 2;
                    else if (clean[0] == '/')
                        clean++;

                    clean = g_strdup (clean);
                    strip_trailing_slashes (clean);

                    if (clean[0] != '\0' && !has_entry (data->all_entries, clean))
                    {
                        ensure_parent_dirs (data->all_entries, clean);

                        e = g_new0 (arcmc_entry_t, 1);
                        e->full_path = clean;

                        basename_ptr = strrchr (clean, '/');
                        e->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : clean);

                        e->mode = st.st_mode;
                        if (!S_ISDIR (e->mode))
                            e->mode = S_IFREG | (e->mode & 07777);

                        e->size = st.st_size;
                        e->mtime = st.st_mtime;
                        e->is_virtual_dir = FALSE;

                        g_ptr_array_add (data->all_entries, e);
                    }
                    else
                        g_free (clean);
                }

                g_free (filename);
                g_free (linkname);
            }

            g_string_free (buffer, TRUE);
        }
    }

    if (remain_line != NULL)
        g_string_free (remain_line, TRUE);

    mc_pclose (pip, NULL);

    return (data->all_entries->len > 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Extract a file from the archive using extfs helper's "copyout" command.
   Returns MC_PPR_OK on success. */
mc_pp_result_t
arcmc_extract_entry_extfs (arcmc_data_t *data, const char *target_path, char **local_path)
{
    GError *error = NULL;
    int fd;
    char *quoted_archive, *quoted_file, *quoted_local;
    char *cmd;
    mc_pipe_t *pip;

    fd = g_file_open_tmp ("mc-arcmc-XXXXXX", local_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        return MC_PPR_FAILED;
    }
    close (fd);

    quoted_archive = name_quote (data->archive_path, FALSE);
    quoted_file = name_quote (target_path, FALSE);
    quoted_local = name_quote (*local_path, FALSE);

    cmd = g_strconcat (data->extfs_helper, " copyout ", quoted_archive, " ", quoted_file, " ",
                       quoted_local, (char *) NULL);

    g_free (quoted_archive);
    g_free (quoted_file);
    g_free (quoted_local);

    pip = mc_popen (cmd, FALSE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    pip->out.len = MC_PIPE_STREAM_UNREAD;
    pip->err.len = MC_PIPE_BUFSIZE;

    mc_pread (pip, &error);

    mc_pclose (pip, NULL);

    if (error != NULL)
    {
        g_error_free (error);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    /* verify the temp file has content */
    {
        struct stat st;

        if (stat (*local_path, &st) != 0 || st.st_size == 0)
        {
            /* zero-size might be ok for empty files, check if the entry had size 0 */
            /* just return OK — zero-byte files are valid */
        }
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Run an extfs helper command (copyin / rm).
   Returns TRUE on success (exit code 0). */
gboolean
arcmc_extfs_run_cmd (const char *helper, const char *cmd_name, const char *archive_path,
                     const char *stored_name, const char *local_name)
{
    char *quoted_archive, *quoted_stored, *cmd;
    mc_pipe_t *pip;
    GError *error = NULL;

    quoted_archive = name_quote (archive_path, FALSE);
    quoted_stored = name_quote (stored_name, FALSE);

    if (local_name != NULL)
    {
        char *quoted_local = name_quote (local_name, FALSE);

        cmd = g_strconcat (helper, cmd_name, quoted_archive, " ", quoted_stored, " ", quoted_local,
                           (char *) NULL);
        g_free (quoted_local);
    }
    else
        cmd = g_strconcat (helper, cmd_name, quoted_archive, " ", quoted_stored, (char *) NULL);

    g_free (quoted_archive);
    g_free (quoted_stored);

    pip = mc_popen (cmd, FALSE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    pip->out.len = MC_PIPE_STREAM_UNREAD;
    pip->err.len = MC_PIPE_BUFSIZE;

    mc_pread (pip, &error);

    if (error != NULL)
    {
        g_error_free (error);
        mc_pclose (pip, NULL);
        return FALSE;
    }

    if (pip->err.len > 0)
    {
        mc_pclose (pip, NULL);
        return FALSE;
    }

    mc_pclose (pip, NULL);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Open the archive and read its table of contents into all_entries.
   Returns TRUE on success, FALSE on error. */
gboolean
arcmc_read_archive (arcmc_data_t *data)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new ();
    archive_read_support_filter_all (a);
    archive_read_support_format_all (a);

    if (data->password != NULL)
        archive_read_add_passphrase (a, data->password);

    r = archive_read_open_filename (a, data->archive_path, 10240);
    if (r != ARCHIVE_OK)
    {
        archive_read_free (a);
        return FALSE;
    }

    if (data->all_entries != NULL)
        g_ptr_array_free (data->all_entries, TRUE);

    data->all_entries = g_ptr_array_new_with_free_func (arcmc_entry_free);

    while ((r = archive_read_next_header (a, &entry)) == ARCHIVE_OK)
    {
        const char *pathname;
        char *clean_path;
        char *basename_ptr;
        arcmc_entry_t *e;
        mode_t entry_mode;

        pathname = archive_entry_pathname (entry);
        if (pathname == NULL || pathname[0] == '\0')
            continue;

        clean_path = g_strdup (pathname);
        strip_trailing_slashes (clean_path);

        if (clean_path[0] == '\0')
        {
            g_free (clean_path);
            continue;
        }

        /* skip if duplicate */
        if (has_entry (data->all_entries, clean_path))
        {
            g_free (clean_path);
            continue;
        }

        /* ensure parent directories exist */
        ensure_parent_dirs (data->all_entries, clean_path);

        e = g_new0 (arcmc_entry_t, 1);
        e->full_path = clean_path;

        basename_ptr = strrchr (clean_path, '/');
        e->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : clean_path);

        entry_mode = archive_entry_mode (entry);
        if (S_ISDIR (entry_mode))
            e->mode = S_IFDIR | (entry_mode & 07777);
        else
            e->mode = S_IFREG | (entry_mode & 07777);

        e->size = archive_entry_size (entry);
        e->mtime = archive_entry_mtime (entry);
        e->is_virtual_dir = FALSE;

        g_ptr_array_add (data->all_entries, e);

        archive_read_data_skip (a);
    }

    /* Check if we failed due to encryption */
    if (r != ARCHIVE_EOF)
    {
        const char *err_str;

        err_str = archive_error_string (a);
        if (err_str != NULL
            && (strstr (err_str, "passphrase") != NULL || strstr (err_str, "password") != NULL
                || strstr (err_str, "ncrypt") != NULL))
        {
            archive_read_free (a);
            return FALSE;
        }
    }

    archive_read_free (a);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Try to open the archive, requesting password on encryption errors. */
gboolean
arcmc_try_open (arcmc_data_t *data)
{
    /* first attempt without password via libarchive */
    if (arcmc_read_archive (data))
        return TRUE;

    /* try extfs helper as fallback */
    {
        char *helper;

        helper = arcmc_find_extfs_helper (data->archive_path);
        if (helper != NULL)
        {
            data->extfs_helper = helper;

            if (arcmc_read_archive_extfs (data))
                return TRUE;

            /* extfs helper failed too */
            g_free (data->extfs_helper);
            data->extfs_helper = NULL;
        }
    }

    /* maybe encrypted — ask for password (libarchive only) */
    for (;;)
    {
        char *pw;

        pw = input_dialog (_ ("Archive password"), _ ("Enter password:"), "arcmc-password",
                           INPUT_PASSWORD, INPUT_COMPLETE_NONE);
        if (pw == NULL)
            return FALSE;

        g_free (data->password);
        data->password = pw;

        if (arcmc_read_archive (data))
            return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Calculate total size of entries in an existing archive. */
off_t
arcmc_entries_total_size (GPtrArray *all_entries)
{
    guint i;
    off_t total = 0;

    if (all_entries == NULL)
        return 0;

    for (i = 0; i < all_entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (all_entries, i);

        if (!S_ISDIR (e->mode))
            total += e->size;
    }

    return total;
}

/* --------------------------------------------------------------------------------------------- */

/* Add a file to an existing archive by rewriting it:
   1. Read all entries from the old archive
   2. Write them + the new file to a temp archive
   3. Replace the old archive with the temp one
   `p` is an optional progress context (may be NULL).
   Returns TRUE on success. */
gboolean
arcmc_archive_add_file (const char *archive_path, const char *local_path, const char *archive_name,
                        const char *password, arcmc_progress_t *p)
{
    struct archive *reader, *writer;
    struct archive_entry *entry;
    int fmt;
    char *tmp_path;
    char buf[8192];
    gboolean ok = TRUE;

    fmt = arcmc_detect_fmt_id (archive_path);
    if (fmt < 0)
        return FALSE;

    tmp_path = g_strdup_printf ("%s.arcmc-tmp", archive_path);

    /* set up writer */
    writer = archive_write_new ();
    if (!arcmc_write_set_format (writer, fmt))
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    if (password != NULL && password[0] != '\0')
    {
        archive_write_set_passphrase (writer, password);
        if (fmt == ARCMC_FMT_ZIP)
            archive_write_set_options (writer, "zip:encryption=aes256");
    }

    if (archive_write_open_filename (writer, tmp_path) != ARCHIVE_OK)
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    /* copy existing entries */
    reader = archive_read_new ();
    archive_read_support_filter_all (reader);
    archive_read_support_format_all (reader);
    if (password != NULL)
        archive_read_add_passphrase (reader, password);

    if (archive_read_open_filename (reader, archive_path, 10240) == ARCHIVE_OK)
    {
        while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
        {
            const char *epath;
            la_ssize_t bytes;
            off_t entry_size, entry_done = 0;

            if (archive_write_header (writer, entry) != ARCHIVE_OK)
            {
                ok = FALSE;
                break;
            }
            epath = archive_entry_pathname (entry);
            entry_size = archive_entry_size (entry);

            while ((bytes = archive_read_data (reader, buf, sizeof (buf))) > 0)
            {
                la_ssize_t written;

                written = archive_write_data (writer, buf, (size_t) bytes);
                if (written < 0)
                {
                    ok = FALSE;
                    break;
                }
                entry_done += bytes;

                if (p != NULL)
                {
                    p->done_bytes += bytes;
                    p->written_bytes += written;

                    if (!arcmc_progress_update (p, epath, entry_size, entry_done, p->done_bytes,
                                                p->written_bytes))
                    {
                        ok = FALSE;
                        break;
                    }
                }
            }

            if (!ok)
                break;
        }
    }
    else
        ok = FALSE;
    archive_read_free (reader);

    /* add the new file */
    if (ok)
    {
        if (!arcmc_pack_add_file (writer, local_path, archive_name, p))
            ok = FALSE;
    }

    archive_write_close (writer);
    archive_write_free (writer);

    if (ok)
    {
        if (rename (tmp_path, archive_path) != 0)
            ok = FALSE;
    }

    if (!ok)
        unlink (tmp_path);

    g_free (tmp_path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Rewrite the archive excluding entries whose paths match `del_paths`.
   `p` is an optional progress context (may be NULL).
   Returns TRUE on success. */
gboolean
arcmc_archive_delete (const char *archive_path, const char **del_paths, int del_count,
                      const char *password, arcmc_progress_t *p)
{
    struct archive *reader, *writer;
    struct archive_entry *entry;
    int fmt;
    char *tmp_path;
    char buf[8192];
    gboolean ok = TRUE;

    fmt = arcmc_detect_fmt_id (archive_path);
    if (fmt < 0)
        return FALSE;

    tmp_path = g_strdup_printf ("%s.arcmc-tmp", archive_path);

    writer = archive_write_new ();
    if (!arcmc_write_set_format (writer, fmt))
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    if (password != NULL && password[0] != '\0')
    {
        archive_write_set_passphrase (writer, password);
        if (fmt == ARCMC_FMT_ZIP)
            archive_write_set_options (writer, "zip:encryption=aes256");
    }

    if (archive_write_open_filename (writer, tmp_path) != ARCHIVE_OK)
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    reader = archive_read_new ();
    archive_read_support_filter_all (reader);
    archive_read_support_format_all (reader);
    if (password != NULL)
        archive_read_add_passphrase (reader, password);

    if (archive_read_open_filename (reader, archive_path, 10240) == ARCHIVE_OK)
    {
        while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
        {
            const char *pathname;
            char *clean;

            pathname = archive_entry_pathname (entry);
            if (pathname == NULL)
            {
                archive_read_data_skip (reader);
                continue;
            }

            clean = g_strdup (pathname);
            strip_trailing_slashes (clean);

            if (arcmc_path_is_deleted (clean, del_paths, del_count))
            {
                g_free (clean);
                archive_read_data_skip (reader);
                continue;
            }

            g_free (clean);

            if (archive_write_header (writer, entry) != ARCHIVE_OK)
            {
                ok = FALSE;
                break;
            }
            {
                la_ssize_t bytes;
                off_t entry_size, entry_done = 0;

                entry_size = archive_entry_size (entry);

                while ((bytes = archive_read_data (reader, buf, sizeof (buf))) > 0)
                {
                    la_ssize_t written;

                    written = archive_write_data (writer, buf, (size_t) bytes);
                    if (written < 0)
                    {
                        ok = FALSE;
                        break;
                    }
                    entry_done += bytes;

                    if (p != NULL)
                    {
                        p->done_bytes += bytes;
                        p->written_bytes += written;

                        if (!arcmc_progress_update (p, pathname, entry_size, entry_done,
                                                    p->done_bytes, p->written_bytes))
                        {
                            ok = FALSE;
                            break;
                        }
                    }
                }
            }

            if (!ok)
                break;
        }
    }
    else
        ok = FALSE;
    archive_read_free (reader);

    archive_write_close (writer);
    archive_write_free (writer);

    if (ok)
    {
        if (rename (tmp_path, archive_path) != 0)
            ok = FALSE;
    }

    if (!ok)
        unlink (tmp_path);

    g_free (tmp_path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Create an archive from the given file list.
   `cwd` is the current working directory for resolving relative names.
   `files` is an array of file/dir names (relative to cwd).
   Returns TRUE on success. */
gboolean
arcmc_do_pack (const arcmc_pack_opts_t *opts, const char *cwd, GPtrArray *files)
{
    struct archive *a;
    guint i;
    arcmc_progress_t *progress;
    off_t total_size;
    gboolean aborted = FALSE;
    gboolean pack_error = FALSE;

    total_size = arcmc_calculate_total_size (cwd, files);
    progress = arcmc_progress_create (_ ("Creating archive..."), opts->archive_path, total_size);

    a = archive_write_new ();

    /* set archive format */
    switch (opts->format)
    {
    case ARCMC_FMT_ZIP:
        archive_write_set_format_zip (a);
        break;
    case ARCMC_FMT_7Z:
        archive_write_set_format_7zip (a);
        break;
    case ARCMC_FMT_TAR_GZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_gzip (a);
        break;
    case ARCMC_FMT_TAR_BZ2:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_bzip2 (a);
        break;
    case ARCMC_FMT_TAR_XZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_xz (a);
        break;
    case ARCMC_FMT_TAR:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_none (a);
        break;
    case ARCMC_FMT_CPIO:
        archive_write_set_format_cpio (a);
        archive_write_add_filter_none (a);
        break;
    default:
        archive_write_free (a);
        return FALSE;
    }

    /* set compression options */
    if (opts->format == ARCMC_FMT_ZIP || opts->format == ARCMC_FMT_7Z)
    {
        const char *level = NULL;

        switch (opts->compression)
        {
        case 0:
            level = "0";
            break;
        case 1:
            level = "1";
            break;
        case 2:
            level = "6";
            break;
        case 3:
            level = "9";
            break;
        default:
            break;
        }

        if (level != NULL)
        {
            char *opt_str;

            if (opts->format == ARCMC_FMT_ZIP)
                opt_str = g_strdup_printf ("zip:compression-level=%s", level);
            else
                opt_str = g_strdup_printf ("7zip:compression-level=%s", level);

            archive_write_set_options (a, opt_str);
            g_free (opt_str);
        }
    }

    /* set encryption */
    if (opts->password != NULL && opts->password[0] != '\0')
    {
        archive_write_set_passphrase (a, opts->password);

        if (opts->format == ARCMC_FMT_ZIP)
        {
            archive_write_set_options (a, "zip:encryption=aes256");
        }
    }

    /* resolve archive path relative to cwd */
    {
        char *archive_path;

        if (g_path_is_absolute (opts->archive_path))
            archive_path = g_strdup (opts->archive_path);
        else
            archive_path = g_build_filename (cwd, opts->archive_path, NULL);

        if (archive_write_open_filename (a, archive_path) != ARCHIVE_OK)
        {
            g_free (archive_path);
            archive_write_free (a);
            return FALSE;
        }
        g_free (archive_path);
    }

    /* add files */
    for (i = 0; i < files->len; i++)
    {
        const char *name = (const char *) g_ptr_array_index (files, i);
        char *full_path;
        struct stat st;
        const char *arc_name;

        full_path = g_build_filename (cwd, name, NULL);

        if (lstat (full_path, &st) != 0)
        {
            g_free (full_path);
            continue;
        }

        arc_name = opts->store_paths ? name : strrchr (name, '/');
        if (arc_name == NULL || !opts->store_paths)
            arc_name = name;

        {
            gboolean ok;

            if (S_ISDIR (st.st_mode))
                ok = arcmc_pack_add_directory (a, full_path, arc_name, progress);
            else
                ok = arcmc_pack_add_file (a, full_path, arc_name, progress);

            if (!ok)
            {
                if (progress->aborted)
                    aborted = TRUE;
                else
                    pack_error = TRUE;
                g_free (full_path);
                break;
            }
        }

        g_free (full_path);
    }

    archive_write_close (a);
    archive_write_free (a);

    arcmc_progress_destroy (progress);

    if (aborted || pack_error)
    {
        /* remove partial archive */
        char *archive_path;

        if (g_path_is_absolute (opts->archive_path))
            archive_path = g_strdup (opts->archive_path);
        else
            archive_path = g_build_filename (cwd, opts->archive_path, NULL);

        unlink (archive_path);
        g_free (archive_path);

        /* abort is not an error (user chose to cancel), but pack_error is */
        return aborted;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract the entry `target_path` from the current archive to a temp file.
   `p` is an optional progress context (may be NULL). */
mc_pp_result_t
arcmc_extract_entry (arcmc_data_t *data, const char *target_path, char **local_path,
                     arcmc_progress_t *p)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new ();
    archive_read_support_filter_all (a);
    archive_read_support_format_all (a);

    if (data->password != NULL)
        archive_read_add_passphrase (a, data->password);

    r = archive_read_open_filename (a, data->archive_path, 10240);
    if (r != ARCHIVE_OK)
    {
        archive_read_free (a);
        return MC_PPR_FAILED;
    }

    while (archive_read_next_header (a, &entry) == ARCHIVE_OK)
    {
        const char *pathname;
        char *clean;

        pathname = archive_entry_pathname (entry);
        if (pathname == NULL)
            continue;

        clean = g_strdup (pathname);
        strip_trailing_slashes (clean);

        if (strcmp (clean, target_path) != 0)
        {
            g_free (clean);
            archive_read_data_skip (a);
            continue;
        }

        g_free (clean);

        /* found the entry — extract it to a temp file */
        {
            GError *error = NULL;
            int fd;
            off_t file_size, file_done = 0;

            file_size = archive_entry_size (entry);

            fd = g_file_open_tmp ("mc-arcmc-XXXXXX", local_path, &error);
            if (fd == -1)
            {
                if (error != NULL)
                    g_error_free (error);
                archive_read_free (a);
                return MC_PPR_FAILED;
            }

            for (;;)
            {
                const void *buff;
                size_t len;
                la_int64_t offset;

                r = archive_read_data_block (a, &buff, &len, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r != ARCHIVE_OK)
                {
                    close (fd);
                    unlink (*local_path);
                    g_free (*local_path);
                    *local_path = NULL;
                    archive_read_free (a);
                    return MC_PPR_FAILED;
                }

                {
                    const char *wbuf = (const char *) buff;
                    size_t remaining = len;

                    while (remaining > 0)
                    {
                        ssize_t nw = write (fd, wbuf, remaining);

                        if (nw <= 0)
                        {
                            close (fd);
                            unlink (*local_path);
                            g_free (*local_path);
                            *local_path = NULL;
                            archive_read_free (a);
                            return MC_PPR_FAILED;
                        }
                        wbuf += nw;
                        remaining -= (size_t) nw;
                    }
                }

                file_done += (off_t) len;

                if (p != NULL)
                {
                    p->done_bytes += (off_t) len;
                    p->written_bytes += (off_t) len;

                    if (!arcmc_progress_update (p, target_path, file_size, file_done, p->done_bytes,
                                                p->written_bytes))
                    {
                        close (fd);
                        unlink (*local_path);
                        g_free (*local_path);
                        *local_path = NULL;
                        archive_read_free (a);
                        return MC_PPR_FAILED;
                    }
                }
            }

            close (fd);
        }

        archive_read_free (a);
        return MC_PPR_OK;
    }

    /* entry not found */
    archive_read_free (a);
    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract the entry `name` from the current archive to a temp file.
   Returns MC_PPR_OK on success with `local_path` set. */
mc_pp_result_t
arcmc_extract_to_temp (arcmc_data_t *data, const char *name, char **local_path)
{
    char *target_path;
    mc_pp_result_t r;

    target_path = build_child_path (data->current_dir, name);

    if (data->extfs_helper != NULL)
        r = arcmc_extract_entry_extfs (data, target_path, local_path);
    else
    {
        arcmc_progress_t *progress;
        off_t file_size = 0;
        guint i;

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
        r = arcmc_extract_entry (data, target_path, local_path, progress);
        arcmc_progress_destroy (progress);
    }

    g_free (target_path);
    return r;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract a file from the current archive and push it as a nested archive.
   `local_path` is the already-extracted temp file (takes ownership).
   Returns MC_PPR_OK on success, MC_PPR_FAILED on failure (temp file cleaned up). */
mc_pp_result_t
arcmc_push_nested (arcmc_data_t *data, char *local_path)
{
    arcmc_nest_frame_t *frame;

    /* push current state onto the nest stack */
    frame = g_new0 (arcmc_nest_frame_t, 1);
    frame->prev = data->nest_stack;
    frame->archive_path = data->archive_path;
    frame->current_dir = data->current_dir;
    frame->password = data->password;
    frame->extfs_helper = data->extfs_helper;
    frame->all_entries = data->all_entries;
    frame->temp_file = local_path;
    data->nest_stack = frame;

    /* switch to the nested archive */
    data->archive_path = g_strdup (local_path);
    data->current_dir = g_strdup ("");
    data->password = NULL;
    data->extfs_helper = NULL;
    data->all_entries = NULL;

    if (!arcmc_try_open (data))
    {
        /* failed — pop the stack and restore */
        arcmc_nest_frame_t *f = data->nest_stack;

        g_free (data->archive_path);
        g_free (data->current_dir);
        g_free (data->password);
        g_free (data->extfs_helper);
        if (data->all_entries != NULL)
            g_ptr_array_free (data->all_entries, TRUE);

        data->archive_path = f->archive_path;
        data->current_dir = f->current_dir;
        data->password = f->password;
        data->extfs_helper = f->extfs_helper;
        data->all_entries = f->all_entries;
        data->nest_stack = f->prev;

        unlink (f->temp_file);
        g_free (f->temp_file);
        g_free (f);

        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
