/** \file layout.h
 *  \brief Header: panel layout module
 */

#ifndef MC__LAYOUT_H
#define MC__LAYOUT_H

#include "lib/global.h"
#include "lib/widget.h"

#include "panel.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern int winch_flag;
extern int equal_split;
extern int first_panel_size;
extern int output_lines;
extern int command_prompt;
extern int menubar_visible;
extern int keybar_visible;
extern int output_start_y;
extern int message_visible;
extern int xterm_title;
extern int free_space;

extern int horizontal_split;
extern int nice_rotating_dash;

/*** declarations of public functions ************************************************************/

void layout_change (void);
void layout_box (void);
void setup_panels (void);
void destroy_panels (void);
void sigwinch_handler (int dummy);
void change_screen_size (void);
void set_display_type (int num, panel_view_mode_t type);
void panel_update_cols (Widget * widget, panel_display_t frame_size);
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
const char *get_panel_dir_for (const WPanel * widget);

void set_hintbar (const char *str);

/* Rotating dash routines */
void use_dash (gboolean flag);  /* Disable/Enable rotate_dash routines */
void rotate_dash (void);

/* Clear screen */
void clr_scr (void);
void repaint_screen (void);
void mc_refresh (void);

void print_vfs_message (const char *msg, ...) __attribute__ ((format (__printf__, 1, 2)));

/*** inline functions ****************************************************************************/

#endif /* MC__LAYOUT_H */
