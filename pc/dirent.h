/*
 *  direct.h    Defines the types and structures used by the directory routines
 *
 */
#ifndef _DIRENT_H_incl
#define _DIRENT_H_incl

#ifdef __WATCOMC__
#include <direct.h>

#else

#ifdef __cplupplus
extern "C" {
#endif

#include <sys/types.h>

#ifdef __os2__
#ifndef __OS2_H__
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#include <os2.h>
#endif
#endif /* __os2__ */

#define NAME_MAX        255             /* maximum filename for HPFS or NTFS */

typedef struct dirent {
    unsigned long* d_handle;
    unsigned	d_attr;                 /* file's attribute */
    unsigned short int d_time;          /* file's time */
    unsigned short int d_date;          /* file's date */
    long        d_size;                 /* file's size */
    char        d_name[ NAME_MAX + 1 ]; /* file's name */
    unsigned short d_ino;               /* serial number (not used) */
    char        d_first;                /* flag for 1st time */
} DIR;

#ifndef _A_NORMAL
#define _A_NORMAL       0x00    /* Normal file - read/write permitted */
#define _A_RDONLY       0x01    /* Read-only file */
#define _A_HIDDEN       0x02    /* Hidden file */
#define _A_SYSTEM       0x04    /* System file */
#define _A_VOLID        0x08    /* Volume-ID entry */
#define _A_SUBDIR       0x10    /* Subdirectory */
#define _A_ARCH         0x20    /* Archive file */
#endif /* _A_NORMAL_ */

extern int      closedir( DIR * );
/*
extern char     *getcwd( char *__buf, unsigned __size );
extern unsigned _getdrive( void );
extern unsigned _getdiskfree( unsigned __drive, struct _diskfree_t *__diskspace);
*/
extern DIR      *opendir( const char * );
extern struct dirent *readdir( DIR * );
#if !defined(__os2__)
extern int      chdir( const char *__path );
extern int      mkdir( const char *__path );
extern int      rmdir( const char *__path );
#endif

#ifdef __cplusplus
};
#endif

#endif  /* __WATCOMC__ */

#endif /* _DIRENT_H_incl */
