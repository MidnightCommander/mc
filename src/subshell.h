
/** \file subshell.h
 *  \brief Header: concurrent shell support
 */

#ifndef MC_SUBSHELL_H
#define MC_SUBSHELL_H

/* Used to distinguish between a normal MC termination and */
/* one caused by typing `exit' or `logout' in the subshell */
#define SUBSHELL_EXIT 128

#ifdef HAVE_SUBSHELL_SUPPORT

/* File descriptor of the pseudoterminal used by the subshell */
extern int subshell_pty;

/* State of the subshell; see subshell.c for an explanation */
enum subshell_state_enum {INACTIVE, ACTIVE, RUNNING_COMMAND};
extern enum subshell_state_enum subshell_state;

/* Holds the latest prompt captured from the subshell */
extern char *subshell_prompt;

/* For the `how' argument to various functions */
enum {QUIETLY, VISIBLY};

/* Exported functions */
void init_subshell (void);
int invoke_subshell (const char *command, int how, char **new_dir);
int read_subshell_prompt (void);
void resize_subshell (void);
int exit_subshell (void);
void do_subshell_chdir (const char *directory, int update_prompt, int reset_prompt);
void subshell_get_console_attributes (void);
void sigchld_handler (int sig);

void do_update_prompt (void);

#endif /* not HAVE_SUBSHELL_SUPPORT */

#endif
