#ifndef __BACKGROUND_H
#define __BACKGROUND_H

/* Background code requires socketpair to be available */
/* Do not add the background code if it is not supported */
#ifdef USE_NETCODE
#        define WITH_BACKGROUND
#endif

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

extern int background_wait;

int do_background (FileOpContext *ctx, char *info);
void tell_parent (int msg);
int parent_call (void *routine, FileOpContext *ctx, int argc, ...);

void unregister_task_running (pid_t, int fd);

/* stubs */
void message_1s (int flags, char *title, char *str1);
void message_2s (int flags, char *title, char *str1, char *str2);
void message_3s (int flags, char *title, char *str1, char *str2, const char *str3);
void message_1s1d (int flags, char *title, char *str, int d);

#endif
