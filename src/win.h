#ifndef MC_WIN_H
#define MC_WIN_H

/* Keys management */
typedef void (*movefn) (const void *, int);
int check_movement_keys (int key, int page_size, const void *data, movefn backfn,
			 movefn forfn, movefn topfn, movefn bottomfn);
int lookup_key (char *keyname);

/* Terminal management */
extern int xterm_flag;
void do_enter_ca_mode (void);
void do_exit_ca_mode (void);

void mc_raw_mode (void);
void mc_noraw_mode (void);

#endif
