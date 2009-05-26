
/** \file key.h
 *  \brief Header: keyboard support routines
 */

#ifndef MC_KEY_H
#define MC_KEY_H

#include "../../src/global.h"		/* <glib.h> */
#include "../../src/dialog.h"		/* cb_ret_t */

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

/* Keys management */
typedef void (*move_fn) (void *data, int param);
cb_ret_t check_movement_keys (int key, int page_size, void *data,
				move_fn backfn, move_fn forfn,
				move_fn topfn, move_fn bottomfn);
int lookup_keyname (char *keyname);
int lookup_key (char *keyname);

typedef const struct {
    int code;
    const char *name;
    const char *longname;
} key_code_name_t;

extern key_code_name_t key_name_conv_tab [];

/* mouse support */
struct Gpm_Event;
int get_event (struct Gpm_Event *event, int redo_event, int block);
int is_idle (void);
int mi_getch (void);

/* Possible return values from get_event: */
#define EV_MOUSE   -2
#define EV_NONE    -1


/*
 * Internal representation of the key modifiers.  It is used in the
 * sequence tables and the keycodes in the mc sources.
 */
#define KEY_M_SHIFT 0x1000
#define KEY_M_ALT   0x2000
#define KEY_M_CTRL  0x4000
#define KEY_M_MASK  0x7000

extern int double_click_speed;
extern int old_esc_mode;
extern int use_8th_bit_as_meta;
extern int mou_auto_repeat;

/* While waiting for input, the program can select on more than one file */
typedef int (*select_fn)(int fd, void *info);

/* Channel manipulation */
void add_select_channel    (int fd, select_fn callback, void *info);
void delete_select_channel (int fd);
void remove_select_channel (int fd);

/* Activate/deactivate the channel checking */
void channels_up (void);
void channels_down (void);

gboolean is_abort_char (int c);

#define XCTRL(x) (KEY_M_CTRL | ((x) & 31))
#define ALT(x) (KEY_M_ALT | (unsigned int)(x))

/* To define sequences and return codes */
#define MCKEY_NOACTION	0
#define MCKEY_ESCAPE	1

/* Return code for the mouse sequence */
#define MCKEY_MOUSE     -2

int define_sequence (int code, const char *seq, int action);

/* internally used in key.c, defined in keyxtra.c */
void load_xtra_key_defines (void);

/* Learn a single key */
char *learn_key (void);

/* Returns a key code (interpreted) */
int get_key_code (int nodelay);

/* Set keypad mode (xterm and linux console only) */
void numeric_keypad_mode (void);
void application_keypad_mode (void);

#endif				/* MC_KEY_H */
