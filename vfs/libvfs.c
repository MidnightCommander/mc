/*
 * These are functions that miss from vfs.c to make it complete library
 */

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

void vfs_init( void );
void ftpfs_init_passwd( void );

void
mc_vfs_init( void )
{
vfs_init();
ftpfs_init_passwd();
}

void
mc_vfs_done( void )
{
vfs_shut();
}
