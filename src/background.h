#ifndef __BACKGROUND_H
#define __BACKGROUND_H

/* Used for parent/child communication.  These are numbers that
 * could not possible be a routine address.
 */
enum {
    MSG_CHILD_EXITING
};

enum ReturnType {
    Return_String,
    Return_Integer
};

enum TaskState {
    Task_Running,
    Task_Stopped
};

typedef struct TaskList {
    int   fd;
    pid_t pid;
    int   state;
    char  *info;
    struct TaskList *next;
} TaskList;

extern struct TaskList *task_list;

void tell_parent (int msg);

struct FileOpContext;
int do_background (struct FileOpContext *ctx, char *info);
int parent_call (void *routine, struct FileOpContext *ctx, int argc, ...);

void unregister_task_running (pid_t, int fd);

/* Show message box, background safe */
void mc_message (int flags, char *title, const char *text, ...)
    __attribute__ ((format (printf, 3, 4)));

#endif
