/*
   paths to configuration files

   Copyright (C) 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"           /* unix_error_string() */

#include "lib/mcconfig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MC_OLD_USERCONF_DIR ".mc"

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean xdg_vars_initialized = FALSE;
static char *mc_config_str = NULL;
static char *mc_cache_str = NULL;
static char *mc_data_str = NULL;

static const char *homedir = NULL;
/* value of $MC_HOME */
static const char *mc_home = NULL;

static gboolean config_dir_present = FALSE;

static const struct
{
    const char *old_filename;

    char **new_basedir;
    const char *new_filename;
} mc_config_files_reference[] =
{
    /* *INDENT-OFF* */
    /* config */
    { "ini",                                   &mc_config_str, MC_CONFIG_FILE},
    { "filehighlight.ini",                     &mc_config_str, MC_FHL_INI_FILE},
    { "hotlist",                               &mc_config_str, MC_HOTLIST_FILE},
    { "mc.keymap",                             &mc_config_str, GLOBAL_KEYMAP_FILE},
    { "menu",                                  &mc_config_str, MC_USERMENU_FILE},
    { "cedit" PATH_SEP_STR "Syntax",           &mc_config_str, EDIT_SYNTAX_FILE},
    { "cedit" PATH_SEP_STR "menu",             &mc_config_str, EDIT_HOME_MENU},
    { "cedit" PATH_SEP_STR "edit.indent.rc",   &mc_config_str, EDIT_DIR PATH_SEP_STR "edit.indent.rc"},
    { "cedit" PATH_SEP_STR "edit.spell.rc",    &mc_config_str, EDIT_DIR PATH_SEP_STR "edit.spell.rc"},
    { "panels.ini",                            &mc_config_str, MC_PANELS_FILE},

    /* User should move this file with applying some changes in file */
    { "",                                      &mc_config_str, MC_FILEBIND_FILE},

    /* data */
    { "skins",                                 &mc_data_str, MC_SKINS_SUBDIR},
    { "fish",                                  &mc_data_str, FISH_PREFIX},
    { "bashrc",                                &mc_data_str, "bashrc"},
    { "inputrc",                               &mc_data_str, "inputrc"},
    { "extfs.d",                               &mc_data_str, MC_EXTFS_DIR},
    { "history",                               &mc_data_str, MC_HISTORY_FILE},
    { "filepos",                               &mc_data_str, MC_FILEPOS_FILE},
    { "cedit" PATH_SEP_STR "cooledit.clip",    &mc_data_str, EDIT_CLIP_FILE},
    { "",                                      &mc_data_str, MC_MACRO_FILE},

    /* cache */
    { "log",                                   &mc_cache_str, "mc.log"},
    { "Tree",                                  &mc_cache_str, MC_TREESTORE_FILE},
    { "cedit" PATH_SEP_STR "cooledit.temp",    &mc_cache_str, EDIT_TEMP_FILE},
    { "cedit" PATH_SEP_STR "cooledit.block",   &mc_cache_str, EDIT_BLOCK_FILE},

    {NULL, NULL, NULL}
    /* *INDENT-ON* */
};

#ifdef MC_HOMEDIR_XDG
static const struct
{
    char **old_basedir;
    const char *filename;

    char **new_basedir;
} mc_config_migrate_rules_fix[] =
{
    /* *INDENT-OFF* */
    { &mc_data_str, MC_USERMENU_FILE,                       &mc_config_str},
    { &mc_data_str, EDIT_SYNTAX_FILE,                       &mc_config_str},
    { &mc_data_str, EDIT_HOME_MENU,                         &mc_config_str},
    { &mc_data_str, EDIT_DIR PATH_SEP_STR "edit.indent.rc", &mc_config_str},
    { &mc_data_str, EDIT_DIR PATH_SEP_STR "edit.spell.rc",  &mc_config_str},
    { &mc_data_str, MC_FILEBIND_FILE,                       &mc_config_str},

    { &mc_cache_str, MC_HISTORY_FILE,                       &mc_data_str},
    { &mc_cache_str, MC_FILEPOS_FILE,                       &mc_data_str},
    { &mc_cache_str, EDIT_CLIP_FILE,                        &mc_data_str},

    { &mc_cache_str, MC_PANELS_FILE,                        &mc_config_str},

    {NULL, NULL, NULL}
    /* *INDENT-ON* */
};
#endif /* MC_HOMEDIR_XDG */

/*** file scope functions *********************************************************************** */
/* --------------------------------------------------------------------------------------------- */

static void
mc_config_mkdir (const char *directory_name, GError ** error)
{
    if ((!g_file_test (directory_name, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) &&
        (g_mkdir_with_parents (directory_name, 0700) != 0))
    {
        g_propagate_error (error,
                           g_error_new (MC_ERROR, 0, _("Cannot create %s directory"),
                                        directory_name));
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_config_init_one_config_path (const char *path_base, const char *subdir, GError ** error)
{
    char *full_path;

    full_path = g_build_filename (path_base, subdir, NULL);

    if (g_file_test (full_path, G_FILE_TEST_EXISTS))
    {
        if (g_file_test (full_path, G_FILE_TEST_IS_DIR))
        {
            config_dir_present = TRUE;
        }
        else
        {
            fprintf (stderr, "%s %s\n", _("FATAL: not a directory:"), full_path);
            exit (EXIT_FAILURE);
        }
    }

    mc_config_mkdir (full_path, error);
    if (error != NULL && *error != NULL)
    {
        g_free (full_path);
        full_path = NULL;
    }
    return full_path;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_config_get_deprecated_path (void)
{
    return g_build_filename (mc_config_get_home_dir (), MC_OLD_USERCONF_DIR, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_config_copy (const char *old_name, const char *new_name, GError ** error)
{
    if (g_file_test (old_name, G_FILE_TEST_IS_REGULAR))
    {
        char *contents = NULL;
        size_t length;

        if (g_file_get_contents (old_name, &contents, &length, error))
            g_file_set_contents (new_name, contents, length, error);

        g_free (contents);
        return;
    }

    if (g_file_test (old_name, G_FILE_TEST_IS_DIR))
    {

        GDir *dir;
        const char *dir_name;

        dir = g_dir_open (old_name, 0, error);
        if (dir == NULL)
            return;

        if (g_mkdir_with_parents (new_name, 0700) == -1)
        {
            g_dir_close (dir);
            g_propagate_error (error,
                               g_error_new (MC_ERROR, 0,
                                            _
                                            ("An error occurred while migrating user settings: %s"),
                                            unix_error_string (errno)));
            return;
        }

        while ((dir_name = g_dir_read_name (dir)) != NULL)
        {
            char *old_name2, *new_name2;

            old_name2 = g_build_filename (old_name, dir_name, NULL);
            new_name2 = g_build_filename (new_name, dir_name, NULL);
            mc_config_copy (old_name2, new_name2, error);
            g_free (new_name2);
            g_free (old_name2);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

#if MC_HOMEDIR_XDG
static void
mc_config_fix_migrated_rules (void)
{
    size_t rule_index;

    for (rule_index = 0; mc_config_migrate_rules_fix[rule_index].old_basedir != NULL; rule_index++)
    {
        char *old_name;

        old_name =
            g_build_filename (*mc_config_migrate_rules_fix[rule_index].old_basedir,
                              mc_config_migrate_rules_fix[rule_index].filename, NULL);

        if (g_file_test (old_name, G_FILE_TEST_EXISTS))
        {
            char *new_name;
            const char *basedir = *mc_config_migrate_rules_fix[rule_index].new_basedir;
            const char *filename = mc_config_migrate_rules_fix[rule_index].filename;

            new_name = g_build_filename (basedir, filename, NULL);
            rename (old_name, new_name);
            g_free (new_name);
        }
        g_free (old_name);
    }
}
#endif /* MC_HOMEDIR_XDG */

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_config_deprecated_dir_present (void)
{
    char *old_dir;
    gboolean is_present;

    old_dir = mc_config_get_deprecated_path ();
    is_present = g_file_test (old_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
    g_free (old_dir);

    return is_present && !config_dir_present;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_config_init_config_paths (GError ** error)
{
    char *dir;

    if (xdg_vars_initialized)
        return;

    /* init mc_home and homedir if not yet */
    (void) mc_config_get_home_dir ();

#ifdef MC_HOMEDIR_XDG
    if (mc_home != NULL)
    {
        dir = g_build_filename (mc_home, ".config", (char *) NULL);
        mc_config_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        g_free (dir);

        dir = g_build_filename (mc_home, ".cache", (char *) NULL);
        mc_cache_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        g_free (dir);

        dir = g_build_filename (mc_home, ".local", "share", (char *) NULL);
        mc_data_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        g_free (dir);
    }
    else
    {
        dir = (char *) g_get_user_config_dir ();
        if (dir != NULL && *dir != '\0')
            mc_config_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        else
        {
            dir = g_build_filename (homedir, ".config", (char *) NULL);
            mc_config_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
            g_free (dir);
        }

        dir = (char *) g_get_user_cache_dir ();
        if (dir != NULL && *dir != '\0')
            mc_cache_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        else
        {
            dir = g_build_filename (homedir, ".cache", (char *) NULL);
            mc_cache_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
            g_free (dir);
        }

        dir = (char *) g_get_user_data_dir ();
        if (dir != NULL && *dir != '\0')
            mc_data_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
        else
        {
            dir = g_build_filename (homedir, ".local", "share", (char *) NULL);
            mc_data_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, error);
            g_free (dir);
        }
    }

    mc_config_fix_migrated_rules ();
#else /* MC_HOMEDIR_XDG */
    char *defined_userconf_dir;

    defined_userconf_dir = tilde_expand (MC_USERCONF_DIR);
    if (g_path_is_absolute (defined_userconf_dir))
        dir = defined_userconf_dir;
    else
    {
        g_free (defined_userconf_dir);
        dir = g_build_filename (mc_config_get_home_dir (), MC_USERCONF_DIR, (char *) NULL);
    }

    mc_data_str = mc_cache_str = mc_config_str = mc_config_init_one_config_path (dir, "", error);

    g_free (dir);
#endif /* MC_HOMEDIR_XDG */

    xdg_vars_initialized = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_config_deinit_config_paths (void)
{
    if (!xdg_vars_initialized)
        return;

    g_free (mc_config_str);
#ifdef MC_HOMEDIR_XDG
    g_free (mc_cache_str);
    g_free (mc_data_str);
#endif /* MC_HOMEDIR_XDG */

    g_free (mc_global.share_data_dir);
    g_free (mc_global.sysconfig_dir);

    xdg_vars_initialized = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_data_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_data_str;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_cache_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_cache_str;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_home_dir (void)
{
    if (homedir == NULL)
    {
        homedir = g_getenv ("MC_HOME");
        if (homedir == NULL || *homedir == '\0')
            homedir = g_getenv ("HOME");
        else
            mc_home = homedir;
        if (homedir == NULL || *homedir == '\0')
            homedir = g_get_home_dir ();
    }
    return homedir;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_config_str;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_config_migrate_from_old_place (GError ** error, char **msg)
{
    char *old_dir;
    size_t rule_index;

    if (!mc_config_deprecated_dir_present ())
        return FALSE;

    old_dir = mc_config_get_deprecated_path ();

    g_free (mc_config_init_one_config_path (mc_config_str, EDIT_DIR, error));
#ifdef MC_HOMEDIR_XDG
    g_free (mc_config_init_one_config_path (mc_cache_str, EDIT_DIR, error));
    g_free (mc_config_init_one_config_path (mc_data_str, EDIT_DIR, error));
#endif /* MC_HOMEDIR_XDG */

    if (*error != NULL)
        return FALSE;

    for (rule_index = 0; mc_config_files_reference[rule_index].old_filename != NULL; rule_index++)
    {
        char *old_name;
        if (*mc_config_files_reference[rule_index].old_filename == '\0')
            continue;

        old_name =
            g_build_filename (old_dir, mc_config_files_reference[rule_index].old_filename, NULL);

        if (g_file_test (old_name, G_FILE_TEST_EXISTS))
        {
            char *new_name;
            const char *basedir = *mc_config_files_reference[rule_index].new_basedir;
            const char *filename = mc_config_files_reference[rule_index].new_filename;

            new_name = g_build_filename (basedir, filename, NULL);
            mc_config_copy (old_name, new_name, error);
            g_free (new_name);
        }
        g_free (old_name);
    }

#ifdef MC_HOMEDIR_XDG
    *msg = g_strdup_printf (_("Your old settings were migrated from %s\n"
                              "to Freedesktop recommended dirs.\n"
                              "To get more info, please visit\n"
                              "http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html"),
                            old_dir);
#else /* MC_HOMEDIR_XDG */
    *msg = g_strdup_printf (_("Your old settings were migrated from %s\n"
                              "to %s\n"), old_dir, mc_config_str);
#endif /* MC_HOMEDIR_XDG */

    g_free (old_dir);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get full path to config file by short name.
 *
 * @param config_name short name
 * @return full path to config file
 */

char *
mc_config_get_full_path (const char *config_name)
{
    size_t rule_index;

    if (config_name == NULL)
        return NULL;

    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    for (rule_index = 0; mc_config_files_reference[rule_index].old_filename != NULL; rule_index++)
    {
        if (strcmp (config_name, mc_config_files_reference[rule_index].new_filename) == 0)
        {
            return g_build_filename (*mc_config_files_reference[rule_index].new_basedir,
                                     mc_config_files_reference[rule_index].new_filename, NULL);
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get full path to config file by short name.
 *
 * @param config_name short name
 * @return object with full path to config file
 */

vfs_path_t *
mc_config_get_full_vpath (const char *config_name)
{
    vfs_path_t *ret_vpath;
    char *str_path;

    str_path = mc_config_get_full_path (config_name);

    ret_vpath = vfs_path_from_str (str_path);
    g_free (str_path);
    return ret_vpath;
}

/* --------------------------------------------------------------------------------------------- */
