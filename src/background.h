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

/* stubs */
void message_1s (int flags, char *title, const char *str1);
void message_2s (int flags, char *title, const char *str1, const char *str2);
void message_3s (int flags, char *title, const char *str1, const char *str2,
		 const char *str3);
void message_1s1d (int flags, char *title, const char *str, int d);

#endif
