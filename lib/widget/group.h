/*
 *  Widget group features module for Midnight Commander
 */

/** \file group.h
 *  \brief Header: widget group features module
 */

#ifndef MC__GROUP_H
#define MC__GROUP_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define GROUP(x) ((WGroup *)(x))
#define CONST_GROUP(x) ((const WGroup *)(x))

/*** enums ***************************************************************************************/

/*** typedefs(not structures) ********************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WGroup
{
    Widget widget;

    /* Group members */
    GList *widgets;             /* widgets list */
    GList *current;             /* Currently active widget */
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#endif /* MC__GROUP_H */
