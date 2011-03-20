/** \file hotlist.h
 *  \brief Header: directory hotlist
 */

#ifndef MC__HOTLIST_H
#define MC__HOTLIST_H

/*** typedefs(not structures) and defined constants **********************************************/

#define LIST_VFSLIST    0x01
#define LIST_HOTLIST    0x02
#define LIST_MOVELIST   0x04

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void add2hotlist_cmd (void);
char *hotlist_show (int list_vfs);
int save_hotlist (void);
void done_hotlist (void);

/*** inline functions ****************************************************************************/
#endif /* MC__HOTLIST_H */
