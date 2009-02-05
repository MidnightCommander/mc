
/** \file cons.saver.h
 *  \brief Header: general purpose Linux console screen save/restore server
 *
 *  This code does _not_ need to be setuid root. However, it needs
 *  read/write access to /dev/vcsa* (which is priviledged
 *  operation). You should create user vcsa, make cons.saver setuid
 *  user vcsa, and make all vcsa's owned by user vcsa.
 *  Seeing other peoples consoles is bad thing, but believe me, full
 *  root is even worse.
 */

#ifndef MC_CONS_SAVER_H
#define MC_CONS_SAVER_H

enum {
    CONSOLE_INIT = '1',
    CONSOLE_DONE,
    CONSOLE_SAVE,
    CONSOLE_RESTORE,
    CONSOLE_CONTENTS
};

#ifndef LINUX_CONS_SAVER_C
/* Used only in mc, not in cons.saver */

extern signed char console_flag;

void show_console_contents (int starty, unsigned char begin_line, unsigned char end_line);
void handle_console (unsigned char action);

void show_rxvt_contents (int starty, unsigned char y1, unsigned char y2);
int look_for_rxvt_extensions (void);

extern int cons_saver_pid;
#endif /* !LINUX_CONS_SAVER_C */

#endif
