/* linklist.h */
#if !defined(__LINKLIST_H)
#define __LINKLIST_H

struct linklist {
    void *data;
    struct linklist *next;
    struct linklist *prev;
};

struct LRU_list {
    struct LRU_list *prev;
    struct LRU_list *next;
    void *data;
};

struct list_iterator {
    struct linklist *linklist;
    struct linklist *current_pos;
};

struct linklist *linklist_init(void);
void linklist_destroy(struct linklist *, void (*destructor) (void *));
int linklist_insert(struct linklist *, void *);
int linklist_delete(struct linklist *, void *);
void linklist_delete_all(struct linklist *, void (*) (void *));

#endif




