#ifndef CALLBACK_H
#define CALLBACK_H

/* 
 * All callbacks are char *func( char *msg );
 * INFO/BOX should always return NULL;
 */

#define CALL_INFO 0
#define CALL_BOX 1
#define CALL_PASSWD 2

#define NUM_CALLBACKS 3

extern void vfs_set_callback( int num, void *func );

#endif 
