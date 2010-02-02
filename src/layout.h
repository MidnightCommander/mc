
/** \file layout.h
 *  \brief Header: panel layout module
 */

#ifndef MC_LAYOUT_H
#define MC_LAYOUT_H

#include "panel.h"
#include "widget.h"

void layout_cmd (void);
void setup_panels (void);
void destroy_panels (void);
void sigwinch_handler (int dummy);
void change_screen_size (void);
void set_display_type (int num, panel_view_mode_t type);
void swap_panels (void);
panel_view_mode_t get_display_type (int idx);
panel_view_mode_t get_current_type (void);
panel_view_mode_t get_other_type (void);
int get_current_index (void);
int get_other_index (void);
const char *get_nth_panel_name (int num);

struct Widget *get_panel_widget (int idx);

struct WPanel *get_other_panel (void);

void save_panel_dir (int idx);
Widget *restore_into_right_dir_panel (int idx, Widget *from_widget);
const char *get_panel_dir_for (const WPanel *widget);

void set_hintbar (const char *str);

/* Clear screen */
void clr_scr (void);
void repaint_screen (void);
void mc_refresh (void);

extern int winch_flag;
extern int equal_split;
extern int first_panel_size;
extern int output_lines;
extern int command_prompt;
extern int keybar_visible;
extern int output_start_y;
extern int message_visible;
extern int xterm_title;
extern int free_space;

extern int horizontal_split;
extern int nice_rotating_dash;

#endif
