#ifndef __CMD_H
#define __CMD_H

void netlink_cmd (void);
void ftplink_cmd (void);
void fishlink_cmd (void);
void smblink_cmd (void);
void undelete_cmd (void);
void help_cmd (void);
void dirsizes_cmd (void);
int view_file_at_line (char *filename, int plain_view, int internal,
		       int start_line);
int view_file (char *filename, int normal, int internal);
void view_cmd (void);
void view_file_cmd (void);
void view_simple_cmd (void);
void filtered_view_cmd (void);
void do_edit_at_line (const char *what, int start_line);
void edit_cmd (void);
void edit_cmd_new (void);
void copy_cmd (void);
void ren_cmd (void);
void copy_cmd_local (void);
void ren_cmd_local (void);
void delete_cmd_local (void);
void free_vfs_now (void);
void reselect_vfs (void);
void mkdir_cmd (void);
void delete_cmd (void);
void find_cmd (void);
void tree_mode_cmd (void);
void filter_cmd (void);
void reread_cmd (void);
void tree_view_cmd (void);
void ext_cmd (void);
void edit_mc_menu_cmd (void);
void edit_user_menu_cmd (void);
void edit_syntax_cmd (void);
void quick_chdir_cmd (void);
void compare_dirs_cmd (void);
void history_cmd (void);
void tree_cmd (void);
void link_cmd (void);
void symlink_cmd (void);
void edit_symlink_cmd (void);
void reverse_selection_cmd (void);
void unselect_cmd (void);
void select_cmd (void);
void swap_cmd (void);
void view_other_cmd (void);
void quick_cd_cmd (void);
void save_setup_cmd (void);
char *get_random_hint (int force);
void source_routing (void);
void user_file_menu_cmd (void);
char *guess_message_value (void);
void info_cmd (void);
void listing_cmd (void);
void quick_cmd_no_menu (void);
void info_cmd_no_menu (void);
void quick_view_cmd (void);
void toggle_listing_cmd (void);

#endif /* __CMD_H */
