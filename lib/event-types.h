#ifndef MC__EVENT_TYPES_H
#define MC__EVENT_TYPES_H

#include <stdarg.h>

/*** typedefs(not structures) and defined constants **********************************************/

/* Event groups for main modules */
#define MCEVENT_GROUP_CORE "Core"
#define MCEVENT_GROUP_DIFFVIEWER "DiffViewer"
#define MCEVENT_GROUP_EDITOR "Editor"
#define MCEVENT_GROUP_FILEMANAGER "FileManager"
#define MCEVENT_GROUP_VIEWER "Viewer"

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/


/* MCEVENT_GROUP_CORE:vfs_timestamp */
struct vfs_class;
typedef struct
{
    struct vfs_class *vclass;
    gpointer id;
    gboolean ret;
} ev_vfs_stamp_create_t;


/* MCEVENT_GROUP_CORE:vfs_print_message */
typedef struct
{
    const char *msg;
    va_list ap;
} ev_vfs_print_message_t;


/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC__EVENT_TYPES_H */
