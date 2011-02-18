/** \file subshell.h
 *  \brief Header: concurrent shell support
 */

#ifndef MC__SUBSHELL_H
#define MC__SUBSHELL_H

/*** typedefs(not structures) and defined constants **********************************************/

/* Used to distinguish between a normal MC termination and */
/* one caused by typing `exit' or `logout' in the subshell */
#define SUBSHELL_EXIT 128

#ifdef HAVE_SUBSHELL_SUPPORT

/*** enums ***************************************************************************************/

/* State of the subshell; see subshell.c for an explanation */

enum subshell_state_enum
{
    INACTIVE,
    ACTIVE,
    RUNNING_COMMAND
};

/* For the `how' argument to various functions */
enum
{
    QUIETLY,
    VISIBLY
};

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern enum subshell_state_enum subshell_state;

/* Holds the latest prompt captured from the subshell */
extern char *subshell_prompt;

extern gboolean update_subshell_prompt;

/*** declarations of public functions ************************************************************/

void init_subshell (void);
int invoke_subshell (const char *command, int how, char **new_dir);
int read_subshell_prompt (void);
void do_update_prompt (void);
int exit_subshell (void);
void do_subshell_chdir (const char *directory, gboolean update_prompt, gboolean reset_prompt);
void subshell_get_console_attributes (void);
void sigchld_handler (int sig);

#endif /* HAVE_SUBSHELL_SUPPORT */

/*** inline functions ****************************************************************************/

#endif /* MC__SUBSHELL_H */
