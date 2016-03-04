/** \file background.h
 *  \brief Header: Background support
 */

#ifndef MC__BACKGROUND_H
#define MC__BACKGROUND_H

#include <sys/types.h>          /* pid_t */
#include "filemanager/fileopctx.h"
/*** typedefs(not structures) and defined constants **********************************************/

enum TaskState
{
    Task_Running,
    Task_Stopped
};

typedef struct TaskList
{
    int fd;
    int to_child_fd;
    pid_t pid;
    int state;
    char *info;
    struct TaskList *next;
} TaskList;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern TaskList *task_list;

/*** declarations of public functions ************************************************************/

int do_background (file_op_context_t * ctx, char *info);
int parent_call (void *routine, file_op_context_t * ctx, int argc, ...);
char *parent_call_string (void *routine, int argc, ...);

void unregister_task_running (pid_t pid, int fd);
void unregister_task_with_pid (pid_t pid);

gboolean background_parent_call (const gchar * event_group_name, const gchar * event_name,
                                 gpointer init_data, gpointer data);

gboolean
background_parent_call_string (const gchar * event_group_name, const gchar * event_name,
                               gpointer init_data, gpointer data);

/*** inline functions ****************************************************************************/

#endif /* MC__BACKGROUND_H */
