#include "src/setup.h"
#include "src/subshell.h"
#include "src/main.h"

WPanel *current_panel;
gboolean command_prompt;
enum subshell_state_enum subshell_state;
int quit;

void
sync_tree (const char *path)
{
    (void) path;
}

gboolean
quiet_quit_cmd (void)
{
    return TRUE;
}

char *
expand_format (struct WEdit *edit_widget, char c, gboolean do_quote)
{
    (void) edit_widget;
    (void) c;
    (void) do_quote;

    return NULL;
}

void
shell_execute (const char *command, int flags)
{
    (void) command;
    (void) flags;
}

void
init_subshell (void)
{
}

gboolean
do_load_prompt (void)
{
    return TRUE;
}
