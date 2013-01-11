/** \file cmd.h
 *  \brief Header: routines invoked by a function key
 *
 *  They normally operate on the current panel.
 */

#ifndef MC__CMD_H
#define MC__CMD_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    LINK_HARDLINK = 0,
    LINK_SYMLINK_ABSOLUTE,
    LINK_SYMLINK_RELATIVE
} link_type_t;

/* selection flags */
typedef enum
{
    SELECT_FILES_ONLY = 1 << 0,
    SELECT_MATCH_CASE = 1 << 1,
    SELECT_SHELL_PATTERNS = 1 << 2
} select_flags_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/


#ifdef ENABLE_VFS_FTP
void ftplink_cmd (void);
#endif
#ifdef ENABLE_VFS_SFTP
void sftplink_cmd (void);
#endif
#ifdef ENABLE_VFS_FISH
void fishlink_cmd (void);
#endif
#ifdef ENABLE_VFS_SMB
void smblink_cmd (void);
#endif
void undelete_cmd (void);
void help_cmd (void);
void smart_dirsize_cmd (void);
void single_dirsize_cmd (void);
void dirsizes_cmd (void);
gboolean view_file_at_line (const vfs_path_t * filename_vpath, int plain_view, int internal,
                            long start_line);
gboolean view_file (const vfs_path_t * filename_vpath, int normal, int internal);
void view_cmd (void);
void view_file_cmd (void);
void view_raw_cmd (void);
void view_filtered_cmd (void);
void do_edit_at_line (const vfs_path_t * what_vpath, gboolean internal, long start_line);
void edit_cmd (void);
void edit_cmd_new (void);
#ifdef USE_INTERNAL_EDIT
void edit_cmd_force_internal (void);
#endif
void copy_cmd (void);
void copy_cmd_local (void);
void rename_cmd (void);
void rename_cmd_local (void);
void mkdir_cmd (void);
void delete_cmd (void);
void delete_cmd_local (void);
void find_cmd (void);
void filter_cmd (void);
void reread_cmd (void);
void vfs_list (void);
void ext_cmd (void);
void edit_mc_menu_cmd (void);
void edit_fhl_cmd (void);
void hotlist_cmd (void);
void compare_dirs_cmd (void);
void diff_view_cmd (void);
void panel_tree_cmd (void);
void link_cmd (link_type_t link_type);
void edit_symlink_cmd (void);
void select_invert_cmd (void);
void unselect_cmd (void);
void select_cmd (void);
void swap_cmd (void);
void view_other_cmd (void);
void quick_cd_cmd (void);
void save_setup_cmd (void);
char *get_random_hint (int force);
void user_file_menu_cmd (void);
void info_cmd (void);
void listing_cmd (void);
void change_listing_cmd (void);
void quick_cmd_no_menu (void);
void info_cmd_no_menu (void);
void quick_view_cmd (void);
void toggle_listing_cmd (void);
#ifdef HAVE_CHARSET
void encoding_cmd (void);
#endif

/*** inline functions ****************************************************************************/
#endif /* MC__CMD_H */
