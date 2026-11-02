/*
   cd_to() function.

   Copyright (C) 1995-2026
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2020

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

/** \file cd.c
 *  \brief Source: cd_to() function
 */

#include <config.h>

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"  // whitespace()

#include "src/util.h"  // file_error_message()

#include "filemanager.h"  // current_panel, panel.h, layout.h
#include "tree.h"         // sync_tree()

#include "cd.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Expand the argument to "cd" and change directory.  First try tilde
 * expansion, then variable substitution.  If the CDPATH variable is set
 * (e.g. CDPATH=".:~:/usr"), try all the paths contained there.
 * We do not support such rare substitutions as ${var:-value} etc.
 * No quoting is implemented here, so ${VAR} and $VAR will be always
 * substituted.  Wildcards are not supported either.
 * Advanced users should be encouraged to use "\cd" instead of "cd" if
 * they want the behavior they are used to in the shell.
 *
 * @param _path string to examine
 * @return newly allocated string
 */

static GString *
examine_cd (const char *_path)
{
    typedef enum
    {
        copy_sym,
        subst_var
    } state_t;

    state_t state = copy_sym;
    GString *q;
    char *path_tilde, *path;
    char *p;

    // Tilde expansion
    path = str_shell_unescape (_path);
    path_tilde = tilde_expand (path);
    g_free (path);

    q = g_string_sized_new (32);

    // Variable expansion
    for (p = path_tilde; *p != '\0';)
    {
        switch (state)
        {
        case copy_sym:
            if (p[0] == '\\' && p[1] == '$')
            {
                g_string_append_c (q, '$');
                p += 2;
            }
            else if (p[0] != '$' || p[1] == '[' || p[1] == '(')
            {
                g_string_append_c (q, *p);
                p++;
            }
            else
                state = subst_var;
            break;

        case subst_var:
        {
            char *s = NULL;
            char c;
            const char *t = NULL;

            // skip dollar
            p++;

            if (p[0] == '{')
            {
                p++;
                s = strchr (p, '}');
            }
            if (s == NULL)
                s = strchr (p, PATH_SEP);
            if (s == NULL)
                s = strchr (p, '\0');
            c = *s;
            *s = '\0';
            t = getenv (p);
            *s = c;
            if (t == NULL)
            {
                g_string_append_c (q, '$');
                if (p[-1] != '$')
                    g_string_append_c (q, '{');
            }
            else
            {
                g_string_append (q, t);
                p = s;
                if (*s == '}')
                    p++;
            }

            state = copy_sym;
            break;
        }

        default:
            break;
        }
    }

    g_free (path_tilde);

    return q;
}

/* --------------------------------------------------------------------------------------------- */

/* CDPATH handling */
static gboolean
handle_cdpath (const char *path)
{
    gboolean result = FALSE;

    // CDPATH handling
    if (!IS_PATH_SEP (*path))
    {
        char *cdpath, *p;
        char c;

        cdpath = g_strdup (getenv ("CDPATH"));
        p = cdpath;
        c = (p == NULL) ? '\0' : ':';

        while (!result && c == ':')
        {
            char *s;

            s = strchr (p, ':');
            if (s == NULL)
                s = strchr (p, '\0');
            c = *s;
            *s = '\0';
            if (*p != '\0')
            {
                vfs_path_t *r_vpath;

                r_vpath = vfs_path_build_filename (p, path, (char *) NULL);
                result = panel_cd (current_panel, r_vpath, cd_parse_command);
                vfs_path_free (r_vpath, TRUE);
            }
            *s = c;
            p = s + 1;
        }
        g_free (cdpath);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/** Make a path relative to the current directory of the panel absolute. */

static vfs_path_t *
cd_make_vpath (const char *path)
{
    if (IS_PATH_SEP (*path))
        return vfs_path_from_str (path);
    if (*path != '\0')
        return vfs_path_append_new (current_panel->cwd_vpath, path, (char *) NULL);
    return vfs_path_clone (current_panel->cwd_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static int
cd_compare_names (gconstpointer a, gconstpointer b)
{
    return strcmp (*(const char *const *) a, *(const char *const *) b);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find the directories in @dir_vpath whose names match the shell wildcard @pattern. Like in the
 * shell, wildcards don't match a leading '.'.
 *
 * @return the names of the directories in alphabetical order, NULL if there is none
 */

static GPtrArray *
cd_find_matching_dirs (const vfs_path_t *dir_vpath, const char *pattern)
{
    GPtrArray *names = NULL;
    DIR *dir;
    struct vfs_dirent *dirent;

    dir = mc_opendir (dir_vpath);
    if (dir == NULL)
        return NULL;

    while ((dirent = mc_readdir (dir)) != NULL)
    {
        const char *name = dirent->d_name;
        vfs_path_t *vpath;
        struct stat st;

        if (DIR_IS_DOT (name) || DIR_IS_DOTDOT (name) || fnmatch (pattern, name, FNM_PERIOD) != 0)
            continue;

        // mc_stat() follows symlinks, so links to directories are found too
        vpath = vfs_path_append_new (dir_vpath, name, (char *) NULL);
        if (mc_stat (vpath, &st) == 0 && S_ISDIR (st.st_mode))
        {
            if (names == NULL)
                names = g_ptr_array_new_with_free_func (g_free);
            g_ptr_array_add (names, g_strdup (name));
        }
        vfs_path_free (vpath, TRUE);
    }

    mc_closedir (dir);

    if (names != NULL)
        g_ptr_array_sort (names, cd_compare_names);

    return names;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Expand the shell wildcards ('*', '?' and '[...]') in @path after the first @pos bytes, which
 * are already expanded. Unlike the shell, which expands them to all matching names, every path
 * component with wildcards is replaced with one matching directory: the first one (in
 * alphabetical order) with which the rest of the path can be expanded to a directory too.
 *
 * @return newly allocated expanded path, NULL if it can't be expanded to a directory
 */

static char *
cd_expand_wildcards (const char *path, size_t pos)
{
    const char *w, *start, *end;
    char *prefix, *pattern, *result = NULL;
    size_t prefix_len;
    vfs_path_t *vpath;
    GPtrArray *names;
    guint i;

    w = strpbrk (path + pos, "*?[");
    if (w == NULL)
    {
        struct stat st;

        // everything is expanded: is it a directory?
        vpath = cd_make_vpath (path);
        if (mc_stat (vpath, &st) == 0 && S_ISDIR (st.st_mode))
            result = g_strdup (path);
        vfs_path_free (vpath, TRUE);
        return result;
    }

    for (start = w; start > path && !IS_PATH_SEP (start[-1]); start--)
        ;
    end = strchr (w, PATH_SEP);
    if (end == NULL)
        end = strchr (w, '\0');

    prefix_len = (size_t) (start - path);
    prefix = g_strndup (path, prefix_len);
    pattern = g_strndup (start, (gsize) (end - start));

    vpath = cd_make_vpath (prefix);
    names = cd_find_matching_dirs (vpath, pattern);
    vfs_path_free (vpath, TRUE);

    if (names != NULL)
    {
        for (i = 0; i < names->len && result == NULL; i++)
        {
            const char *name = g_ptr_array_index (names, i);
            char *candidate;

            candidate = g_strconcat (prefix, name, end, (char *) NULL);
            // don't expand the wildcards in the name of the found directory again
            result = cd_expand_wildcards (candidate, prefix_len + strlen (name));
            g_free (candidate);
        }

        g_ptr_array_free (names, TRUE);
    }

    g_free (pattern);
    g_free (prefix);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* Wildcards handling: "cd fo*" changes to the first directory matching "fo*", see
 * cd_expand_wildcards() */
static gboolean
handle_wildcards (const char *path)
{
    gboolean result = FALSE;
    char *expanded;

    if (strpbrk (path, "*?[") == NULL)
        return FALSE;

    expanded = cd_expand_wildcards (path, 0);
    if (expanded != NULL)
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str_flags (expanded, VPF_NO_CANON);
        result = panel_cd (current_panel, vpath, cd_parse_command);
        vfs_path_free (vpath, TRUE);
        g_free (expanded);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Execute the cd command to specified path
 *
 * @param path path to cd
 */

void
cd_to (const char *path)
{
    char *p;

    // Remove leading whitespaces.
    // Any final whitespace should be removed here (to see why, try "cd fred ").
    /* NOTE: I think we should not remove the extra space,
       that way, we can cd into hidden directories */
    /* FIXME: what about interpreting quoted strings like the shell.
       so one could type "cd <tab> M-a <enter>" and it would work. */
    p = g_strstrip (g_strdup (path));

    if (get_current_type () == view_tree)
    {
        vfs_path_t *new_vpath = NULL;

        if (p[0] == '\0')
        {
            new_vpath = vfs_path_from_str (mc_config_get_home_dir ());
            sync_tree (new_vpath);
        }
        else if (DIR_IS_DOTDOT (p))
        {
            if (vfs_path_elements_count (current_panel->cwd_vpath) != 1
                || strlen (vfs_path_get_by_index (current_panel->cwd_vpath, 0)->path) > 1)
            {
                vfs_path_t *tmp_vpath = current_panel->cwd_vpath;

                current_panel->cwd_vpath =
                    vfs_path_vtokens_get (tmp_vpath, 0, vfs_path_tokens_count (tmp_vpath) - 1);
                vfs_path_free (tmp_vpath, TRUE);
            }
            sync_tree (current_panel->cwd_vpath);
        }
        else
        {
            if (IS_PATH_SEP (*p))
                new_vpath = vfs_path_from_str (p);
            else
                new_vpath = vfs_path_append_new (current_panel->cwd_vpath, p, (char *) NULL);

            sync_tree (new_vpath);
        }

        vfs_path_free (new_vpath, TRUE);
    }
    else
    {
        GString *s_path;
        vfs_path_t *q_vpath;
        gboolean ok;

        s_path = examine_cd (p);

        if (s_path->len == 0)
            q_vpath = vfs_path_from_str (mc_config_get_home_dir ());
        else
            q_vpath = vfs_path_from_str_flags (s_path->str, VPF_NO_CANON);

        ok = panel_cd (current_panel, q_vpath, cd_parse_command);
        if (!ok)
            ok = handle_wildcards (s_path->str);
        if (!ok)
            ok = handle_cdpath (s_path->str);
        if (!ok)
        {
            char *d;

            d = vfs_path_to_str_flags (q_vpath, 0, VPF_STRIP_PASSWORD);
            cd_error_message (d);
            g_free (d);
        }

        vfs_path_free (q_vpath, TRUE);
        g_string_free (s_path, TRUE);
    }

    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

void
cd_error_message (const char *path)
{
    file_error_message (_ ("Cannot change directory to\n%s"), path);
}

/* --------------------------------------------------------------------------------------------- */
