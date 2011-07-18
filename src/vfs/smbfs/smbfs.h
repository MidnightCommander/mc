
/**
 * \file
 * \brief Header: Virtual File System: smb file system
 */

#ifndef MC__VFS_SMBFS_H
#define MC__VFS_SMBFS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct smb_authinfo
{
    char *host;
    char *share;
    char *domain;
    char *user;
    char *password;
} smb_authinfo;


/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void init_smbfs (void);
void smbfs_set_debug (int arg);

smb_authinfo *vfs_smb_authinfo_new (const char *host,
                                    const char *share,
                                    const char *domain, const char *user, const char *pass);

/* src/boxes.c */
smb_authinfo *vfs_smb_get_authinfo (const char *host,
                                    const char *share, const char *domain, const char *user);

/*** inline functions ****************************************************************************/
#endif /* MC_VFS_SMBFS_H */
