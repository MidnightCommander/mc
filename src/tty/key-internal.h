
/** \file key-internal.h
 *  \brief Header: keyboard support routines. Internal functions
 */

#ifndef MC_KEY_INTERNAL_H
#define MC_KEY_INTERNAL_H

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

/*** global variables defined in .c file *******************************/

extern int input_fd;

/*** declarations of public functions **********************************/


void tty_key_gsource_init(void);
void tty_key_gsource_deinit(void);

#endif				/* MC_KEY_INTERNAL_H */
