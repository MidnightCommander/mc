/* Session management for the Midnight Commander
 *
 * Copyright (C) 1999 the Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GSESSION_H
#define GSESSION_H


extern int session_have_sm;

void session_init (void);
void session_load (void);
void session_set_restart (int restart);


#endif
