#ifndef MC_YDIFF_H
#define MC_YDIFF_H

typedef struct
{
    int fd;
    int pos;
    int len;
    char *buf;
    int flags;
    void *data;
} FBUF;

typedef struct
{
    int a[2][2];
    int cmd;
} DIFFCMD;

typedef int (*DFUNC) (void *ctx, int ch, int line, off_t off, size_t sz, const char *str);

typedef struct
{
    int off;
    int len;
} BRACKET[2];

typedef int PAIR[2];

typedef enum
{
    DATA_SRC_MEM = 0,
    DATA_SRC_TMP = 1,
    DATA_SRC_ORG = 2
} DSRC;

typedef struct
{
    int ch;
    int line;
    union
    {
        off_t off;
        size_t len;
    } u;
    void *p;
} DIFFLN;

typedef struct
{
    FBUF *f;
    GArray *a;
    DSRC dsrc;
} PRINTER_CTX;

typedef struct
{
    Widget widget;

    const char *args;           /* Args passed to diff */
    const char *file[2];        /* filenames */
    const char *label[2];
    FBUF *f[2];
    const char *backup_sufix;
    gboolean merged;
    GArray *a[2];
    GPtrArray *hdiff;
    int ndiff;                  /* number of hunks */
    DSRC dsrc;                  /* data source: memory or temporary file */

    int view_quit:1;            /* Quit flag */

    int height;
    int half1;
    int half2;
    int width1;
    int width2;
    int bias;
    int new_frame;
    int skip_rows;
    int skip_cols;
    int display_symbols;
    int display_numbers;
    int show_cr;
    int tab_size;
    int ord;
    int full;
    ssize_t last_found;
    gboolean utf8;
    /* converter for translation of text */
    GIConv converter;
    struct
    {
        int quality;
        gboolean strip_trailing_cr;
        gboolean ignore_tab_expansion;
        gboolean ignore_space_change;
        gboolean ignore_all_space;
        gboolean ignore_case;
    } opt;
} WDiff;

typedef enum
{
    DIFF_NONE = 0,
    DIFF_ADD = 1,
    DIFF_DEL = 2,
    DIFF_CHG = 3
} DiffState;

void dview_diff_cmd (WDiff * dview);
int diff_view (const char *file1, const char *file2, const char *label1, const char *label2);

#endif
