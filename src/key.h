#ifndef __KEY_H
#define __KEY_H

#include "mouse.h" /* Gpm_Event */

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

int get_event (Gpm_Event *event, int redo_event, int block);
int is_idle (void);
int ctrl_pressed (void);

int mi_getch (void);
/* Possible return values from get_event: */
#define EV_MOUSE   -2
#define EV_NONE    -1

/* Used to get the modifier information          */
/* Currently, it just works on the Linux console */
#ifdef NATIVE_WIN32
#   ifndef SHIFT_PRESSED
#   define SHIFT_PRESSED 0x0010
#   endif
#else
#   define SHIFT_PRESSED 1
#endif
#define ALTR_PRESSED 2
#define CONTROL_PRESSED 4
#define ALTL_PRESSED 8
int get_modifier (void);


/*
 * Internal representation of the key modifiers.  It is used in the
 * sequence tables.  In the future, it will be part of the key code.
 * Currently those bits are stripped in correct_key_code() and
 * remembered in the last_modifiers variable.
 */
#define KEY_M_SHIFT 0x1000
#define KEY_M_ALT   0x2000
#define KEY_M_CTRL  0x4000
#define KEY_M_MASK  0x7000

extern int double_click_speed;
extern int old_esc_mode;
extern int irix_fn_keys;
extern int use_8th_bit_as_meta;

/* While waiting for input, the program can select on more than one file */

typedef int (*select_fn)(int fd, void *info);

/* Channel manipulation */
void add_select_channel    (int fd, select_fn callback, void *info);
void delete_select_channel (int fd);
void remove_select_channel (int fd);

/* Activate/deactivate the channel checking */
void channels_up (void);
void channels_down (void);

/* Abort/Quit chars */
int is_abort_char (int c);
int is_quit_char (int c);

#define XCTRL(x) ((x) & 31)
#define ALT(x) (0x200 | (unsigned int)(x))

/* To define sequences and return codes */
#define MCKEY_NOACTION	0
#define MCKEY_ESCAPE	1

/* Return code for the mouse sequence */
#define MCKEY_MOUSE     -2

int define_sequence (int code, char *seq, int action);

/* internally used in key.c, defined in keyxtra.c */
void load_xtra_key_defines (void);

/* Learn a single key */
char *learn_key (void);

/* Returns a key code (interpreted) */
int get_key_code (int nodelay);

typedef const struct {
    int code;
    char *name;
    char *longname;
} key_code_name_t;

extern key_code_name_t key_name_conv_tab [];
extern int we_are_background;

/* Set keypad mode (xterm and linux console only) */
void numeric_keypad_mode (void);
void application_keypad_mode (void);

#endif	/* __KEY_H */
