/** \file timer.h
 *  \brief Header: shell structure
 */

#ifndef MC_SHELL_H
#define MC_SHELL_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    BASH,
    ASH_BUSYBOX,                /* BusyBox default shell (ash) */
    DASH,                       /* Debian variant of ash */
    TCSH,
    ZSH,
    FISH
} shell_type_t;


/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    shell_type_t type;
    const char *name;
    char *path;
    char *real_path;
} mc_shell_t;


/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions **************************************************/

#endif /* MC_SHELL_H */
