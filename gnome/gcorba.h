/* CORBA support for the Midhignt Commander
 *
 * Copyright (C) 1999 The Free Sofware Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 *          Elliot Lee <sopwith@cuc.edu>
 */

#ifndef __GCORBA_H
#define __GCORBA_H

#include <orb/orbit.h>


/* The ORB and the POA */
extern CORBA_ORB orb;
PortableServer_POA poa;


int corba_init_server (void);
void corba_activate_server (void);
void corba_create_window (char *dir);


#endif
