
/**
 * \file
 * \brief Header: Virtual File System: smb file system
 */

#ifndef MC_VFS_SMBFS_H
#define MC_VFS_SMBFS_H

void init_smbfs (void);
void smbfs_set_debug (int arg);
void smbfs_set_debugf (const char *filename);

typedef struct smb_authinfo
{
    char *host;
    char *share;
    char *domain;
    char *user;
    char *password;
} smb_authinfo;

smb_authinfo *vfs_smb_authinfo_new (const char *host,
                                    const char *share,
                                    const char *domain,
                                    const char *user,
                                    const char *pass);

/* src/boxes.c */
smb_authinfo *vfs_smb_get_authinfo (const char *host,
                                    const char *share,
                                    const char *domain,
                                    const char *user);

#endif /* MC_VFS_SMBFS_H */
