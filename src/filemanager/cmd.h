/** \file cmd.h
 *  \brief Header: routines invoked by a function key
 *
 *  They normally operate on the current panel.
 */

#ifndef MC__CMD_H
#define MC__CMD_H

#include "lib/global.h"

#include "panel.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    LINK_HARDLINK = 0,
    LINK_SYMLINK_ABSOLUTE,
    LINK_SYMLINK_RELATIVE
} link_type_t;

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
gboolean view_file_at_line (const vfs_path_t * filename_vpath, gboolean plain_view,
                            gboolean internal, long start_line, off_t search_start,
                            off_t search_end);
gboolean view_file (const vfs_path_t * filename_vpath, gboolean plain_view, gboolean internal);
void view_cmd (void);
void view_file_cmd (void);
void view_raw_cmd (void);
void view_filtered_cmd (void);
void edit_file_at_line (const vfs_path_t * what_vpath, gboolean internal, long start_line);
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
void filter_cmd (void);
void reread_cmd (void);
void vfs_list (void);
void ext_cmd (void);
void edit_mc_menu_cmd (void);
void edit_fhl_cmd (void);
void hotlist_cmd (void);
void compare_dirs_cmd (void);
#ifdef USE_DIFF_VIEW
void diff_view_cmd (void);
#endif
void panel_tree_cmd (void);
void link_cmd (link_type_t link_type);
void edit_symlink_cmd (void);
void swap_cmd (void);
void quick_cd_cmd (void);
void save_setup_cmd (void);
void user_file_menu_cmd (void);
void info_cmd (void);
void listing_cmd (void);
void setup_listing_format_cmd (void);
void quick_cmd_no_menu (void);
void info_cmd_no_menu (void);
void quick_view_cmd (void);
#ifdef HAVE_CHARSET
void encoding_cmd (void);
#endif
/* achown.c */
void advanced_chown_cmd (WPanel * panel);
/* chmod.c */
void chmod_cmd (void);
/* chown.c */
void chown_cmd (void);
#ifdef ENABLE_EXT2FS_ATTR
/* chattr.c */
void chattr_cmd (void);
const char *chattr_get_as_str (unsigned long attr);
#endif
/* find.c */
void find_cmd (void);

/*** inline functions ****************************************************************************/
#endif /* MC__CMD_H */
