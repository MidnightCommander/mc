/*
   Internal file viewer for the Midnight Commander
   Syntax highlighting via configurable external command

   Copyright (C) 2026
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file syntax.c
 *  \brief Source: syntax highlighting via external command for mcview
 */

#include <config.h>

#include <string.h>  // strstr(), strlen()

#include "lib/global.h"
#include "lib/widget.h"  // quick_dialog(), message()

#include "syntax.h"

/*** global variables ****************************************************************************/

char *mcview_syntax_command = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *name;        // display name for dialog
    const char *short_name;  // short name for status bar (e.g. "bat", "srchi")
    const char *binary;      // binary to search in PATH
    const char *cmd;         // command template (%s = filename)
} mcview_syntax_backend_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

// Supported syntax highlighting backends, in alphabetical order.
static const mcview_syntax_backend_t syntax_backends[] = {
    { "bat", "bat", "bat", "bat --color=always --style=plain --paging=never %s" },
    { "chroma", "chrm", "chroma", "chroma --formatter terminal256 --fail %s" },
    { "highlight", "hi", "highlight", "highlight -O xterm256 --force %s" },
    { "pygmentize", "pyg", "pygmentize", "pygmentize -f terminal256 -O stripnl=False %s" },
    { "source-highlight", "src-hl", "source-highlight",
      "source-highlight --failsafe --out-format=esc -i %s" },
};

static const size_t syntax_backends_num = G_N_ELEMENTS (syntax_backends);

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_syntax_binary_available (const char *binary)
{
    char *path;

    if (binary == NULL || binary[0] == '\0')
        return FALSE;

    if (binary[0] == '/')
        return g_file_test (binary, G_FILE_TEST_IS_EXECUTABLE);

    path = g_find_program_in_path (binary);
    if (path == NULL)
        return FALSE;

    g_free (path);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

// return the effective syntax command (user-configured or default)
static const char *
mcview_syntax_effective_command (void)
{
    if (mcview_syntax_command != NULL && mcview_syntax_command[0] != '\0')
        return mcview_syntax_command;
    return MCVIEW_SYNTAX_DEFAULT_CMD;
}

/* --------------------------------------------------------------------------------------------- */

// find the backend index matching the current mcview_syntax_command, or -1
static int
mcview_syntax_find_current_backend (void)
{
    const char *current;
    size_t i;

    current = mcview_syntax_effective_command ();

    for (i = 0; i < syntax_backends_num; i++)
        if (strcmp (current, syntax_backends[i].cmd) == 0)
            return (int) i;

    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
mcview_syntax_extract_binary (const char *cmd_template)
{
    const char *p;
    size_t len;

    if (cmd_template == NULL || cmd_template[0] == '\0')
        return NULL;

    // find the first space — everything before it is the binary
    p = strchr (cmd_template, ' ');
    if (p != NULL)
        len = (size_t) (p - cmd_template);
    else
        len = strlen (cmd_template);

    return g_strndup (cmd_template, len);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_syntax_build_command (const char *cmd_template, const char *filename)
{
    const char *p;
    char *quoted_filename;
    GString *result;

    if (cmd_template == NULL || filename == NULL)
        return NULL;

    // check at least one %s exists
    if (strstr (cmd_template, "%s") == NULL)
        return NULL;

    quoted_filename = g_shell_quote (filename);
    result = g_string_new ("");

    // substitute all %s occurrences with the shell-escaped filename
    p = cmd_template;
    while (*p != '\0')
    {
        const char *placeholder;

        placeholder = strstr (p, "%s");
        if (placeholder == NULL)
        {
            g_string_append (result, p);
            break;
        }

        g_string_append_len (result, p, placeholder - p);
        g_string_append (result, quoted_filename);
        p = placeholder + 2;
    }

    g_free (quoted_filename);

    return g_string_free (result, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_syntax_command_available (void)
{
    char *binary;
    gboolean available;

    binary = mcview_syntax_extract_binary (mcview_syntax_effective_command ());
    if (binary == NULL)
        return FALSE;

    available = mcview_syntax_binary_available (binary);
    g_free (binary);

    return available;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcview_syntax_get_short_name (void)
{
    int idx;

    idx = mcview_syntax_find_current_backend ();
    if (idx >= 0)
        return syntax_backends[idx].short_name;

    return "ext";
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_syntax_options_dialog (void)
{
    const char **radio_items;
    int *backend_map;
    int total_items = 0;
    int installed_count = 0;
    int custom_radio_idx;
    int selected = 0;
    int current_idx;
    int qd_result;
    gboolean ret = FALSE;
    size_t i;

    // count installed backends
    for (i = 0; i < syntax_backends_num; i++)
        if (mcview_syntax_binary_available (syntax_backends[i].binary))
            installed_count++;

    // total = installed backends + "Custom..." entry
    total_items = installed_count + 1;
    custom_radio_idx = installed_count;

    // build radio items: installed backends + "Custom..."
    radio_items = g_new (const char *, total_items);
    backend_map = g_new (int, total_items);

    current_idx = mcview_syntax_find_current_backend ();

    {
        int j = 0;

        for (i = 0; i < syntax_backends_num; i++)
        {
            if (!mcview_syntax_binary_available (syntax_backends[i].binary))
                continue;

            radio_items[j] = syntax_backends[i].name;
            backend_map[j] = (int) i;

            if ((int) i == current_idx)
                selected = j;

            j++;
        }

        // add "Custom..." entry
        radio_items[j] = _ ("Custom...");
        backend_map[j] = -1;

        // select "Custom..." if current command doesn't match any backend
        if (current_idx < 0 && mcview_syntax_command != NULL && mcview_syntax_command[0] != '\0')
            selected = j;
    }

    {
        quick_widget_t quick_widgets[] = {
            // clang-format off
            QUICK_RADIO (total_items, radio_items, &selected, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };

        WRect r = { -1, -1, 0, 46 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = _ ("Syntax Backend"),
            .help = "[Internal File Viewer]",
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        qd_result = quick_dialog (&qdlg);
    }

    if (qd_result != B_ENTER)
        goto cleanup;

    if (selected == custom_radio_idx)
    {
        // "Custom..." selected -- prompt for command template
        char *custom_cmd = NULL;
        const char *initial;

        initial = (mcview_syntax_command != NULL) ? mcview_syntax_command : "";

        {
            quick_widget_t custom_widgets[] = {
                // clang-format off
                QUICK_LABELED_INPUT (_ ("Command (%s = filename):"), input_label_above,
                                     initial, "syntax-custom-cmd", &custom_cmd,
                                     NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
                // clang-format on
            };

            WRect cr = { -1, -1, 0, 60 };

            quick_dialog_t custom_dlg = {
                .rect = cr,
                .title = _ ("Custom Syntax Command"),
                .help = "[Internal File Viewer]",
                .widgets = custom_widgets,
                .callback = NULL,
                .mouse_callback = NULL,
            };

            qd_result = quick_dialog (&custom_dlg);
        }

        if (qd_result != B_ENTER || custom_cmd == NULL || custom_cmd[0] == '\0')
        {
            g_free (custom_cmd);
            goto cleanup;
        }

        if (strstr (custom_cmd, "%s") == NULL)
        {
            message (D_ERROR, _ ("Syntax Highlighting"), "%s",
                     _ ("Command must contain %%s placeholder\n"
                        "for the filename."));
            g_free (custom_cmd);
            goto cleanup;
        }

        g_free (mcview_syntax_command);
        mcview_syntax_command = custom_cmd;
        ret = TRUE;
    }
    else
    {
        int chosen;

        g_assert (selected >= 0 && selected < total_items);
        chosen = backend_map[selected];
        g_assert (chosen >= 0 && (size_t) chosen < syntax_backends_num);

        g_free (mcview_syntax_command);
        mcview_syntax_command = g_strdup (syntax_backends[chosen].cmd);
        ret = TRUE;
    }

cleanup:
    g_free (radio_items);
    g_free (backend_map);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
