#include "src/subshell.h"
#include "src/setup.h"


panels_options_t panels_options;
struct mc_fhl_struct *mc_filehighlight;
int confirm_execute = 0;
int auto_save_setup = 0;
int free_space = 0;
int horizontal_split = 0;
int first_panel_size = 0;
int default_source_codepage = 0;
int menubar_visible = 1;
WPanel *current_panel;
WInput *cmdline;
WMenuBar *the_menubar;
const global_keymap_t *panel_map;
gboolean command_prompt;
int saving_setup;

panels_layout_t panels_layout = {
    .horizontal_split = 0,
    .vertical_equal = 1,
    .left_panel_size = 0,
    .horizontal_equal = 1,
    .top_panel_size = 0
};

WInput *
command_new (int y, int x, int cols)
{
    WInput *cmd;
    const input_colors_t command_colors = {
        DEFAULT_COLOR,
        COMMAND_MARK_COLOR,
        DEFAULT_COLOR,
        COMMAND_HISTORY_COLOR
    };

    cmd = input_new (y, x, (int *) command_colors, cols, "", "cmdline",
                     INPUT_COMPLETE_DEFAULT | INPUT_COMPLETE_CD | INPUT_COMPLETE_COMMANDS |
                     INPUT_COMPLETE_SHELL_ESC);

    /* Add our hooks */
    cmd->widget.callback = NULL;

    return cmd;
}

int
do_cd (const vfs_path_t *new_dir, enum cd_enum exact)
{
    (void) new_dir;
    (void) exact;

    return 0;
}

void
do_subshell_chdir (const vfs_path_t * vpath, gboolean update_prompt, gboolean reset_prompt)
{
    (void) vpath;
    (void) update_prompt;
    (void) reset_prompt;
}

void
shell_execute (const char *command, int flags)
{
    (void) command;
    (void) flags;
}

void
panel_load_setup (WPanel * panel, const char *section)
{
    (void) panel;
    (void) section;
}

void
panel_save_setup (WPanel * panel, const char *section)
{
    (void) panel;
    (void) section;
}

void
free_my_statfs (void)
{

}

int
select_charset (int center_y, int center_x, int current_charset, gboolean seldisplay)
{
    (void) center_y;
    (void) center_x;
    (void) current_charset;
    (void) seldisplay;

    return 0;
}

void
update_xterm_title_path (void)
{
}

void
init_my_statfs (void)
{
}
void
my_statfs (struct my_statfs *myfs_stats, const char *path)
{
    (void) myfs_stats;
    (void) path;
}

void
clean_dir (dir_list * list, int count)
{
    (void) list;
    (void) count;

}

struct Widget *
get_panel_widget (int idx)
{
    (void) idx;

    return NULL;
}


int
do_load_dir (const vfs_path_t *vpath, dir_list * list, sortfn * sort, gboolean reverse,
                 gboolean case_sensitive, gboolean exec_ff, const char *fltr)
{
    (void) vpath;
    (void) list;
    (void) sort;
    (void) reverse;
    (void) case_sensitive;
    (void) exec_ff;
    (void) fltr;

    return 0;
}

int
do_reload_dir (const vfs_path_t * vpath, dir_list * list, sortfn * sort, int count,
                   gboolean reverse, gboolean case_sensitive, gboolean exec_ff, const char *fltr)
{
    (void) vpath;
    (void) list;
    (void) sort;
    (void) count;
    (void) reverse;
    (void) case_sensitive;
    (void) exec_ff;
    (void) fltr;

    return 0;

}

void
do_sort (dir_list * list, sortfn * sort, int top, gboolean reverse,
              gboolean case_sensitive, gboolean exec_ff)
{
    (void) list;
    (void) sort;
    (void) top;
    (void) reverse;
    (void) case_sensitive;
    (void) exec_ff;
}

int
regex_command (const vfs_path_t *filename, const char *action, int *move_dir)
{
    (void) filename;
    (void) action;
    (void) move_dir;

    return 0;
}

gboolean
if_link_is_exe (const vfs_path_t *full_name, const file_entry * file)
{
    (void) full_name;
    (void) file;

    return TRUE;
}

void
change_panel (void)
{
}

gboolean
set_zero_dir (dir_list * list)
{
    (void) list;

    return TRUE;
}

void
load_hint (gboolean force)
{
    (void) force;
}

panel_view_mode_t
get_display_type (int idx)
{
    (void) idx;
    return view_listing;
}

panel_view_mode_t
get_current_type (void)
{
    return view_listing;
}

panel_view_mode_t
get_other_type (void)
{
    return view_listing;
}

int
get_current_index (void)
{
    return 0;
}

int
get_other_index (void)
{
    return 1;
}

int
unsorted (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_name (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_vers (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_ext (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_time (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_atime (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_ctime (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_size (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

int
sort_inode (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;

    return 0;
}

void
set_display_type (int num, panel_view_mode_t type)
{
    (void) num;
    (void) type;
}

void
copy_cmd_local (void)
{
}

void
delete_cmd_local (void)
{
}

void
view_raw_cmd (void)
{
}

void
edit_cmd_new (void)
{
}

void
rename_cmd_local (void)
{
}

void
select_invert_cmd (void)
{
}

void
unselect_cmd (void)
{
}

void
select_cmd (void)
{
}

struct WPanel *
get_other_panel (void)
{
    return NULL;
}

const panel_field_t *
sort_box (panel_sort_info_t * info)
{
    (void) info;

    return NULL;
}

void
midnight_set_buttonbar (WButtonBar * b)
{
    (void) b;
}
