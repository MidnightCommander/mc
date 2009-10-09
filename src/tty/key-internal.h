
/** \file key-internal.h
 *  \brief Header: keyboard support routines
 */

#ifndef MC_KEY_INTERNAL_H
#define MC_KEY_INTERNAL_H

#define GET_TIME(tv)		(gettimeofday(&tv, (struct timezone *) NULL))
#define DIF_TIME(t1, t2)	((t2.tv_sec  - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec)/1000)

/* The maximum sequence length (32 + null terminator) */
#define SEQ_BUFFER_LEN 33



typedef struct key_def_struct {
    char ch;                    /* Holds the matching char code */
    tty_key_t code;                   /* The code returned, valid if child == NULL */
    struct key_def_struct *next;
    struct key_def_struct *child;      /* sequence continuation */
    int action;                 /* optional action to be done. Now used only
                                   to mark that we are just after the first
                                   Escape */
} key_def;

extern int *pending_keys;

extern int keyboard_key_timeout;

extern key_def *keys;

extern int input_fd;

tty_key_t correct_key_code (tty_key_t);
tty_key_t tty_key_parse (int);

#endif /* MC_KEY_H */
