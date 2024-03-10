/** \file global.h
 *  \brief Header: %global definitions for compatibility
 *
 *  This file should be included after all system includes and before all local includes.
 */

#ifndef MC_GLOBAL_H
#define MC_GLOBAL_H

#include <glib.h>
#include "glibcompat.h"

#include "unixcompat.h"

#include "fs.h"
#include "shell.h"
#include "mcconfig.h"

/*** typedefs(not structures) and defined constants **********************************************/

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#ifdef gettext_noop
#define N_(String) gettext_noop (String)
#else
#define N_(String) (String)
#endif
#else /* Stubs that do something close enough.  */
#define textdomain(String) 1
#define gettext(String) (String)
#define ngettext(String1,String2,Num) (((Num) == 1) ? (String1) : (String2))
#define dgettext(Domain,Message) (Message)
#define dcgettext(Domain,Message,Type) (Message)
#define bindtextdomain(Domain,Directory) 1
#define _(String) (String)
#define N_(String) (String)
#endif /* !ENABLE_NLS */

#ifdef HAVE_FUNC_ATTRIBUTE_FALLTHROUGH
#define MC_FALLTHROUGH __attribute__((fallthrough))
#else
#define MC_FALLTHROUGH
#endif

#ifdef USE_MAINTAINER_MODE
#include "lib/logging.h"
#endif

/* Just for keeping Your's brains from invention a proper size of the buffer :-) */
#define BUF_10K 10240L
#define BUF_8K  8192L
#define BUF_4K  4096L
#define BUF_1K  1024L

#define BUF_LARGE  BUF_1K
#define BUF_MEDIUM 512
#define BUF_SMALL 128
#define BUF_TINY 64

#define UTF8_CHAR_LEN 6

/* Used to distinguish between a normal MC termination and */
/* one caused by typing 'exit' or 'logout' in the subshell */
#define SUBSHELL_EXIT 128

#define MC_ERROR g_quark_from_static_string (PACKAGE)

#define DEFAULT_CHARSET "ASCII"

/*** enums ***************************************************************************************/

/* run mode and params */
typedef enum
{
    MC_RUN_FULL = 0,
    MC_RUN_EDITOR,
    MC_RUN_VIEWER,
    MC_RUN_DIFFVIEWER
} mc_run_mode_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    const char *mc_version;

    mc_run_mode_t mc_run_mode;
    gboolean run_from_parent_mc;
    /* Used so that widgets know if they are being destroyed or shut down */
    gboolean midnight_shutdown;

    /* sysconfig_dir: Area for default settings from maintainers of distributuves
       default is /etc/mc or may be defined by MC_DATADIR */
    char *sysconfig_dir;
    /* share_data_dir: Area for default settings from developers */
    char *share_data_dir;

    char *profile_name;

    mc_config_t *main_config;
    mc_config_t *panels_config;

#ifdef HAVE_CHARSET
    /* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
    int source_codepage;
    int display_codepage;
#else
    /* If true, allow characters in the range 160-255 */
    gboolean eight_bit_clean;
    /*
     * If true, also allow characters in the range 128-159.
     * This is reported to break on many terminals (xterm, qansi-m).
     */
    gboolean full_eight_bits;
#endif                          /* !HAVE_CHARSET */
    /*
     * If utf-8 terminal utf8_display = TRUE
     * Display bits set UTF-8
     */
    gboolean utf8_display;

    /* Set if the nice message (hint) bar is visible */
    gboolean message_visible;
    /* Set if the nice and useful keybar is visible */
    gboolean keybar_visible;

#ifdef ENABLE_BACKGROUND
    /* If true, this is a background process */
    gboolean we_are_background;
#endif                          /* ENABLE_BACKGROUND */

    struct
    {
        /* Asks for confirmation before clean up of history */
        gboolean confirm_history_cleanup;

        /* Set if you want the possible completions dialog for the first time */
        gboolean show_all_if_ambiguous;

        /* Ugly hack in order to distinguish between left and right panel in menubar */
        /* Set if the command is being run from the "Right" menu */
        gboolean is_right;      /* If the selected menu was the right */
    } widget;

    /* The user's shell */
    mc_shell_t *shell;

    struct
    {
        /* Use the specified skin */
        char *skin;
        /* Dialog window and drop down menu have a shadow */
        gboolean shadows;

        char *setup_color_string;
        char *term_color_string;
        char *color_terminal_string;
        /* colors specified on the command line: they override any other setting */
        char *command_line_colors;

#ifndef LINUX_CONS_SAVER_C
        /* Used only in mc, not in cons.saver */
        char console_flag;
#endif                          /* !LINUX_CONS_SAVER_C */
        /* If using a subshell for evaluating commands this is true */
        gboolean use_subshell;

#ifdef ENABLE_SUBSHELL
        /* File descriptors of the pseudoterminal used by the subshell */
        int subshell_pty;
#endif                          /* !ENABLE_SUBSHELL */

        /* This flag is set by xterm detection routine in function main() */
        /* It is used by function toggle_subshell() */
        gboolean xterm_flag;

        /* disable x11 support */
        gboolean disable_x11;

        /* For slow terminals */
        /* If true lines are shown by spaces */
        gboolean slow_terminal;

        /* Set to force black and white display at program startup */
        gboolean disable_colors;

        /* If true use +, -, | for line drawing */
        gboolean ugly_line_drawing;

        /* Tries to use old highlight mouse tracking */
        gboolean old_mouse;

        /* If true, use + and \ keys normally and select/unselect do if M-+ / M-\.
           and M-- and keypad + / - */
        gboolean alternate_plus_minus;
    } tty;

    struct
    {
        /* Set when cd symlink following is desirable (bash mode) */
        gboolean cd_symlinks;

        /* Preallocate space before file copying */
        gboolean preallocate_space;

    } vfs;
} mc_global_t;

/*** global variables defined in .c file *********************************************************/

extern mc_global_t mc_global;

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/
#endif
