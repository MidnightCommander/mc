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

/* MCEVENT_GROUP_CORE:clipboard_text_from_file */
typedef struct
{
    char **text;
    gboolean ret;
} ev_clipboard_text_from_file_t;

/* MCEVENT_GROUP_CORE:help */
typedef struct
{
    const char *filename;
    const char *node;
} ev_help_t;

/* MCEVENT_GROUP_CORE:background_parent_call */
/* MCEVENT_GROUP_CORE:background_parent_call_string */
typedef struct
{
    void *routine;
    gpointer *ctx;
    int argc;
    va_list ap;
    union
    {
        int i;
        char *s;
    } ret;
} ev_background_parent_call_t;


/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC__EVENT_TYPES_H */
