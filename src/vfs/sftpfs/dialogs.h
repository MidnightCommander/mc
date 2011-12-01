/** \file dialog.h
 *  \brief Header: Configuration dialog of SFTP VFS plugin
 */

#ifndef MC__VFS_SFTP_DIALOGS_H
#define MC__VFS_SFTP_DIALOGS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void configure_sftpfs (void);
void configure_sftpfs_conn (const char *sftpfs_sessionname);

/*** inline functions ****************************************************************************/
#endif /* MC__VFS_SFTP_DIALOGS_H */
