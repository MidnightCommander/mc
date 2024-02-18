/** \file info.h
 *  \brief Header: panel managing
 */

#ifndef MC__INFO_H
#define MC__INFO_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WInfo;
typedef struct WInfo WInfo;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WInfo *info_new (const WRect * r);

/*** inline functions ****************************************************************************/
#endif /* MC__INFO_H */
