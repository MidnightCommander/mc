#ifndef __FILEGUI_H
#define __FILEGUI_H

/*
 * GUI callback routines
 */
FileProgressStatus file_progress_check_buttons (void);
FileProgressStatus file_progress_show          (long done, long total);
FileProgressStatus file_progress_show_count    (long done, long total);
FileProgressStatus file_progress_show_bytes    (double done, double total);
FileProgressStatus file_progress_show_source   (char *path);
FileProgressStatus file_progress_show_target   (char *path);
FileProgressStatus file_progress_show_deleting (char *path);
FileProgressStatus file_progress_real_query_replace (enum OperationMode mode,
						     char *destname,
						     struct stat *_s_stat,
						     struct stat *_d_stat);

void file_progress_set_stalled_label (char *stalled_msg);
void fmd_init_i18n();

/*
 * Shared variables used to pass information from the file.c module to
 * the GUI modules.  Yes, this is hackish.
 *
 * This will be replaced with a FileOpContext soon (which will be created with
 * create_op_win).
 *
 * Now, the reason this has not happened yet is that it needs to be done in
 * sync with the background.c code (status result is passed trough the pipe
 * and this needs to be handled properly, probably by creating a temporary
 * FileOpContext and using this).
 *
 */
extern int    file_progress_replace_result;
extern int    file_progress_recursive_result;
extern char  *file_progress_replace_filename;
extern double file_progress_eta_secs;
extern int    file_progress_do_reget;
extern int    file_progress_do_append;

extern int    file_mask_op_follow_links;
extern int    file_mask_stable_symlinks;
extern int    file_mask_preserve;
extern int    file_mask_preserve_uidgid;
extern int    file_mask_umask_kill;
extern char  *file_mask_dest_mask;

extern unsigned long file_progress_bps;
extern unsigned long file_progress_bps_time;

int (*file_mask_xstat)(char *, struct stat *);

extern struct re_pattern_buffer file_mask_rx;

#ifdef WANT_WIDGETS
char *panel_get_file (WPanel *panel, struct stat *stat_buf);
#endif

#endif
