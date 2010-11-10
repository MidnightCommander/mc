/** \file  clipboard.h
 *  \brief Header: Util for external clipboard
 */

#ifndef MC__CLIPBOARD_H
#define MC__CLIPBOARD_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean copy_file_to_ext_clip (void);
gboolean paste_to_file_from_ext_clip (void);

/*** inline functions ****************************************************************************/
#endif /* MC__CLIPBOARD_H */
