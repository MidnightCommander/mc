#ifndef MC__DIFFVIEW_EXECUTE_H
#define MC__DIFFVIEW_EXECUTE_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

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

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int mc_diffviewer_scan_deci (const char **str, int *n);
int mc_diffviewer_file_close (FBUF * fs);
int mc_diffviewer_execute (const char *args, const char *extra, const char *file1,
                           const char *file2, GArray * ops);
off_t mc_diffviewer_file_seek (FBUF * fs, off_t off, int whence);
size_t mc_diffviewer_file_gets (char *buf, size_t size, FBUF * fs);
off_t mc_diffviewer_file_reset (FBUF * fs);
FBUF *mc_diffviewer_file_open (const char *filename, int flags);
ssize_t mc_diffviewer_file_write (FBUF * fs, const char *buf, size_t size);
off_t mc_diffviewer_file_trunc (FBUF * fs);
FBUF *mc_diffviewer_file_dopen (int fd);
FBUF *mc_diffviewer_file_temp (void);

/*** inline functions ****************************************************************************/

#endif /* MC__DIFFVIEW_EXECUTE_H */
