#ifndef __CONS_SAVER_H
#define __CONS_SAVER_H

#if defined(linux) || defined(__linux__) || defined(_OS_NT) || defined(__os2__)

    enum {
	CONSOLE_INIT = '1',
	CONSOLE_DONE,
	CONSOLE_SAVE,
	CONSOLE_RESTORE,
	CONSOLE_CONTENTS
    };

    extern signed char console_flag;
    void show_console_contents (int starty, unsigned char begin_line, unsigned char end_line);
    void handle_console (unsigned char action);

#else

#   define console_flag 0
#   define show_console_contents(w,f,l) { }
#   define handle_console(x) { } 
    
#endif /* #ifdef linux */

/* Used only in the principal program */
extern int cons_saver_pid;

#endif	/* __CONS_SAVER_H */
