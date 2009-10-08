
/** \file key.h
 *  \brief Header: keyboard support routines
 */

#ifndef MC_KEY_H
#define MC_KEY_H

#include "../../src/global.h"   /* <glib.h> */
#include "../../src/dialog.h"   /* cb_ret_t */

#include "../../src/tty/tty.h"  /* KEY_F macro */

gboolean define_sequence (int code, const char *seq, int action);

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

/* Keys management */
typedef void (*move_fn) (void *data, int param);
cb_ret_t check_movement_keys (int key, int page_size, void *data,
                              move_fn backfn, move_fn forfn, move_fn topfn, move_fn bottomfn);
int lookup_keyname (char *keyname);
int lookup_key (char *keyname);

typedef struct tty_key_struct{
    int modificator;
    gunichar code;
} tty_key_t;

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

/*
 * Internal representation of the key modifiers.  It is used in the
 * sequence tables and the keycodes in the mc sources.
 */

typedef enum {
    KEY_M_INVALID = -1,
    KEY_M_NONE  = 0,
    KEY_M_F     = (1 << 0),
    KEY_M_SHIFT = (1 << 1),
    KEY_M_ALT   = (1 << 2),
    KEY_M_CTRL  = (1 << 3),
} tty_key_modificators_t;

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

#define HOTKEY(x,y) (tty_key_t){x,y}

static inline gboolean
tty_key_compare(tty_key_t *key, int modificator, gunichar code)
{
 if (modificator == KEY_M_NONE || modificator == KEY_M_INVALID)
    return key->code == code;

 if (code == 0)
    return key->modificator == modificator;
  return ((key->code == code) && (key->modificator == modificator));
}

static inline gboolean
tty_key_cmp(tty_key_t *key1, tty_key_t *key2)
{
    return tty_key_compare(key1, key2->modificator, key2->code);
}

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
int get_key_code (int nodelay);

/* Set keypad mode (xterm and linux console only) */
void numeric_keypad_mode (void);
void application_keypad_mode (void);

#endif /* MC_KEY_H */
