#ifndef __LAYOUT_H
#define __LAYOUT_H

void layout_cmd (void);
void init_curses (void);
void done_screen (void);
void setup_panels (void);
void destroy_panels (void);
void move_resize_panel (void);
void flag_winch (int dummy);
void change_screen_size (void);
void set_display_type (int num, int type);
void swap_panels (void);
int get_display_type (int index);
int get_current_type (void);
int get_other_type (void);
int get_current_index (void);
int get_other_index (void);
char *get_nth_panel_name (int num);

struct Widget;
struct Widget *get_panel_widget (int index);

struct WPanel;
struct WPanel *get_other_panel (void);

void set_hintbar (char *str);

/* Clear screen */
void clr_scr (void);

extern int winch_flag;
extern int equal_split;
extern int first_panel_size;
extern int output_lines;
extern int command_prompt;
extern int keybar_visible;
extern int output_start_y;
extern int message_visible;
extern int xterm_title;

extern int horizontal_split;
extern int nice_rotating_dash;

#endif /* __LAYOUT_H */
