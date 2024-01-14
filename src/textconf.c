/*
   Print features specific for this build

   Copyright (C) 2000-2024
   Free Software Foundation, Inc.

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

/** \file textconf.c
 *  \brief Source: prints features specific for this build
 */

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>

#if defined (ENABLE_VFS) && defined(ENABLE_VFS_SFTP)
#include <libssh2.h>
#endif /* ENABLE_VFS_SFTP && ENABLE_VFS */

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"           /* mc_get_profile_root() */
#include "lib/tty/tty.h"        /* S-Lang or ncurses version */

#include "src/textconf.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

#ifdef ENABLE_VFS
static const char *const vfs_supported[] = {
#ifdef ENABLE_VFS_CPIO
    "cpiofs",
#endif
#ifdef ENABLE_VFS_TAR
    "tarfs",
#endif
#ifdef ENABLE_VFS_SFS
    "sfs",
#endif
#ifdef ENABLE_VFS_EXTFS
    "extfs",
#endif
#ifdef ENABLE_VFS_UNDELFS
    "ext2undelfs",
#endif
#ifdef ENABLE_VFS_FTP
    "ftpfs",
#endif
#ifdef ENABLE_VFS_SFTP
    "sftpfs",
#endif
#ifdef ENABLE_VFS_SHELL
    "shell",
#endif
    NULL
};
#endif /* ENABLE_VFS */

static const char *const features[] = {

#ifdef USE_INTERNAL_EDIT
#ifdef HAVE_ASPELL
    N_("With builtin Editor and Aspell support"),
#else
    N_("With builtin Editor"),
#endif /* HAVE_ASPELL */
#endif /* USE_INTERNAL_EDIT */

#ifdef ENABLE_SUBSHELL
#ifdef SUBSHELL_OPTIONAL
    N_("With optional subshell support"),
#else
    N_("With subshell support as default"),
#endif
#endif /* !ENABLE_SUBSHELL */

#ifdef ENABLE_BACKGROUND
    N_("With support for background operations"),
#endif

#ifdef HAVE_LIBGPM
    N_("With mouse support on xterm and Linux console"),
#else
    N_("With mouse support on xterm"),
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    N_("With support for X11 events"),
#endif

#ifdef ENABLE_NLS
    N_("With internationalization support"),
#endif

#ifdef HAVE_CHARSET
    N_("With multiple codepages support"),
#endif

#ifdef ENABLE_EXT2FS_ATTR
    N_("With ext2fs attributes support"),
#endif

    NULL
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
show_version (void)
{
    size_t i;

    printf (_("GNU Midnight Commander %s\n"), mc_global.mc_version);

    printf (_("Built with GLib %d.%d.%d\n"),
            GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);

#ifdef HAVE_SLANG
    printf (_("Built with S-Lang %s with terminfo database\n"), SLANG_VERSION_STRING);
#elif defined(USE_NCURSES)
#ifdef NCURSES_VERSION
    printf (_("Built with ncurses %s\n"), NCURSES_VERSION);
#else
    puts (_("Built with ncurses (unknown version)"));
#endif /* !NCURSES_VERSION */
#elif defined(USE_NCURSESW)
#ifdef NCURSES_VERSION
    printf (_("Built with ncursesw %s\n"), NCURSES_VERSION);
#else
    puts (_("Built with ncursesw (unknown version)"));
#endif /* !NCURSES_VERSION */
#else
#error "Cannot compile mc without S-Lang or ncurses"
#endif /* !HAVE_SLANG && !USE_NCURSES */

#if defined (ENABLE_VFS) && defined(ENABLE_VFS_SFTP)
    printf (_("Built with libssh2 %d.%d.%d\n"),
            LIBSSH2_VERSION_MAJOR, LIBSSH2_VERSION_MINOR, LIBSSH2_VERSION_PATCH);
#endif /* ENABLE_VFS_SFTP && ENABLE_VFS */

    for (i = 0; features[i] != NULL; i++)
        puts (_(features[i]));

#ifdef ENABLE_VFS
    puts (_("Virtual File Systems:"));
    for (i = 0; vfs_supported[i] != NULL; i++)
        printf ("%s %s", i == 0 ? "" : ",", _(vfs_supported[i]));
    (void) puts ("");
#endif /* ENABLE_VFS */

    (void) puts (_("Data types:"));
#define TYPE_INFO(T) \
    (void)printf(" %s: %d;", #T, (int) (CHAR_BIT * sizeof(T)))
    TYPE_INFO (char);
    TYPE_INFO (int);
    TYPE_INFO (long);
    TYPE_INFO (void *);
    TYPE_INFO (size_t);
    TYPE_INFO (off_t);
#undef TYPE_INFO
    (void) puts ("");
}

/* --------------------------------------------------------------------------------------------- */
#define PRINTF_GROUP(a) \
    (void) printf ("[%s]\n", a)
#define PRINTF_SECTION(a,b) \
    (void) printf ("    %-17s %s\n", a, b)
#define PRINTF_SECTION2(a,b) \
    (void) printf ("    %-17s %s/\n", a, b)
#define PRINTF(a, b, c) \
    (void) printf ("\t%-15s %s/%s\n", a, b, c)
#define PRINTF2(a, b, c) \
    (void) printf ("\t%-15s %s%s\n", a, b, c)

void
show_datadirs_extended (void)
{
    (void) printf ("%s %s\n", _("Home directory:"), mc_config_get_home_dir ());
    (void) printf ("%s %s\n", _("Profile root directory:"), mc_get_profile_root ());
    (void) puts ("");

    PRINTF_GROUP (_("System data"));

    PRINTF_SECTION (_("Config directory:"), mc_global.sysconfig_dir);
    PRINTF_SECTION (_("Data directory:"), mc_global.share_data_dir);

    PRINTF_SECTION (_("File extension handlers:"), EXTHELPERSDIR);

#if defined ENABLE_VFS_EXTFS || defined ENABLE_VFS_SHELL
    PRINTF_SECTION (_("VFS plugins and scripts:"), LIBEXECDIR);
#ifdef ENABLE_VFS_EXTFS
    PRINTF2 ("extfs.d:", LIBEXECDIR, MC_EXTFS_DIR PATH_SEP_STR);
#endif
#ifdef ENABLE_VFS_SHELL
    PRINTF2 ("shell:", LIBEXECDIR, VFS_SHELL_PREFIX PATH_SEP_STR);
#endif
#endif /* ENABLE_VFS_EXTFS || defiined ENABLE_VFS_SHELL */
    (void) puts ("");

    PRINTF_GROUP (_("User data"));

    PRINTF_SECTION2 (_("Config directory:"), mc_config_get_path ());
    PRINTF_SECTION2 (_("Data directory:"), mc_config_get_data_path ());
    PRINTF ("skins:", mc_config_get_data_path (), MC_SKINS_DIR PATH_SEP_STR);
#ifdef ENABLE_VFS_EXTFS
    PRINTF ("extfs.d:", mc_config_get_data_path (), MC_EXTFS_DIR PATH_SEP_STR);
#endif
#ifdef ENABLE_VFS_SHELL
    PRINTF ("shell:", mc_config_get_data_path (), VFS_SHELL_PREFIX PATH_SEP_STR);
#endif
#ifdef USE_INTERNAL_EDIT
    PRINTF ("mcedit macros:", mc_config_get_data_path (), MC_MACRO_FILE);
    PRINTF ("mcedit external macros:", mc_config_get_data_path (), EDIT_HOME_MACRO_FILE ".*");
#endif
    PRINTF_SECTION2 (_("Cache directory:"), mc_config_get_cache_path ());
}

#undef PRINTF
#undef PRINTF_SECTION
#undef PRINTF_GROUP

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_CONFIGURE_ARGS
void
show_configure_options (void)
{
    (void) puts (MC_CONFIGURE_ARGS);
}
#endif

/* --------------------------------------------------------------------------------------------- */
