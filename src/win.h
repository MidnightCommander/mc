#ifndef __WIN_H
#define __WIN_H

/* Keys managing */
typedef void (*movefn)(void *, int);
int check_movement_keys (int c, int additional, int page_size, void *,
    movefn backfn, movefn forfn, movefn topfn, movefn bottomfn);
int lookup_key (char *keyname);

/* Terminal managing */
extern int xterm_flag;
void do_enter_ca_mode (void);
void do_exit_ca_mode (void);

void mc_raw_mode (void);
void mc_noraw_mode (void);
#endif	/* __WIN_H */
