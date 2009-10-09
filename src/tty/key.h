
/** \file key.h
 *  \brief Header: keyboard support routines
 */

#ifndef MC_KEY_H
#define MC_KEY_H

#include "../../src/global.h"   /* <glib.h> */
#include "../../src/dialog.h"   /* cb_ret_t */

#include "../../src/tty/tty.h"  /* KEY_F macro */
#include "../../src/tty/keystruct.h"  /* KEY_F macro */

gboolean define_sequence (tty_key_t, const char *, int);

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

/* Keys management */
typedef void (*move_fn) (void *data, int param);
cb_ret_t check_movement_keys (tty_key_t, int, void *, move_fn, move_fn, move_fn, move_fn);
tty_key_t lookup_keyname (const char *);
tty_key_t lookup_key (const char *);

typedef const struct {
    tty_key_t code;
    const char *name;
    const char *longname;
} key_code_name_t;

extern key_code_name_t key_name_conv_tab[];

/* mouse support */
struct Gpm_Event;
int tty_get_event (struct Gpm_Event *event, gboolean redo_event, gboolean block);
gboolean is_idle (void);
int tty_getch (void);

/* Possible return values from tty_get_event: */
#define EV_MOUSE   -2
#define EV_NONE    -1

extern int alternate_plus_minus;
extern int double_click_speed;
extern int old_esc_mode;
extern int use_8th_bit_as_meta;
extern int mou_auto_repeat;

/* While waiting for input, the program can select on more than one file */
typedef int (*select_fn) (int fd, void *info);

/* Channel manipulation */
void add_select_channel (int fd, select_fn callback, void *info);
void delete_select_channel (int fd);
void remove_select_channel (int fd);

/* Activate/deactivate the channel checking */
void channels_up (void);
void channels_down (void);

static inline gboolean
is_abort_char (tty_key_t c)
{
    return (
	tty_key_compare(&c, KEY_M_CTRL, L'c') ||
	tty_key_compare(&c, KEY_M_CTRL, L'g') ||
	tty_key_compare(&c, KEY_M_NONE, ESC_CHAR) ||
	tty_key_compare(&c, KEY_M_F, 10));
}

/* To define sequences and return codes */
#define MCKEY_NOACTION	0
#define MCKEY_ESCAPE	1

/* Return code for the mouse sequence */
#define MCKEY_MOUSE     -2


/* internally used in key.c, defined in keyxtra.c */
void load_xtra_key_defines (void);

/* Learn a single key */
char *learn_key (void);

/* Returns a key code (interpreted) */
tty_key_t get_key_code (int nodelay);

/* Set keypad mode (xterm and linux console only) */
void numeric_keypad_mode (void);
void application_keypad_mode (void);

#endif /* MC_KEY_H */
