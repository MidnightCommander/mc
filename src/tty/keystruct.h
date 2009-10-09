
/** \file key.h
 *  \brief Header: keyboard support routines. Structure of tty_key_t
 */

#ifndef MC_KEYSTRUCT_H
#define MC_KEYSTRUCT_H


typedef struct tty_key_struct{
    int modificator;
    gunichar code;
} tty_key_t;

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


#define HOTKEY(x,y) (tty_key_t){x,y}

static inline gboolean
tty_key_compare(tty_key_t *key, int modificator, gunichar code)
{
 if (modificator == KEY_M_NONE || modificator == KEY_M_INVALID)
    return key->code == code;

 if (code == 0 && modificator != KEY_M_F )
    return key->modificator == modificator;

  return ((key->code == code) && (key->modificator == modificator));
}

static inline gboolean
tty_key_cmp(tty_key_t *key1, tty_key_t *key2)
{
    return tty_key_compare(key1, key2->modificator, key2->code);
}


#endif /* MC_KEYSTRUCT_H */
