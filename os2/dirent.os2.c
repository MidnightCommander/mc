#define INCL_DOSFILEMGR
#define INCL_DOSERRORS

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dirent.h"
#include <errno.h>

DIR *opendir (const char * a_dir)
{
        APIRET          rc;
        FILEFINDBUF3    FindBuffer = {0};
        ULONG           FileCount  = 1;
	DIR             *dd_dir = (DIR*) malloc (sizeof(DIR));
	char            *c_dir = (char*) malloc (strlen(a_dir) + 5);

	strcpy (c_dir, a_dir);
	strcat (c_dir, "\\*.*");
        dd_dir->d_handle = 0xFFFF;  
        
        rc = DosFindFirst(c_dir, 
                         (PHDIR) &dd_dir->d_handle,
                         /* FILE_NORMAL || FILE_DIRECTORY, */
                         FILE_DIRECTORY, 
                         (PVOID) &FindBuffer,
                         sizeof(FILEFINDBUF3),
                         &FileCount,
                         FIL_STANDARD);

        switch (rc) {
        case ERROR_NO_MORE_FILES:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
           errno = ENOENT;
           free(dd_dir);
           return NULL;
           break;
        case ERROR_BUFFER_OVERFLOW:
           errno = ENOMEM;
           free(dd_dir);
           return NULL;
           break;
        case NO_ERROR: /* go through */
           break;
        default: 
           errno = EINVAL;
           free(dd_dir);
           return NULL;
          break;
        } /* endswitch */
	dd_dir->d_attr = FindBuffer.attrFile;
	/* dd_dir->d_attr = (wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
                   		? 0 : wfd.dwFileAttributes; */

	dd_dir->d_time = dd_dir->d_date = 10; 
	dd_dir->d_size = FindBuffer.cbFile;
	strcpy (dd_dir->d_name, FindBuffer.achName);
	dd_dir->d_first = 1;
	
	free (c_dir);
	return dd_dir;
}

DIR *readdir( DIR * dd_dir)
{
        APIRET          rc;
        FILEFINDBUF3    FindBuffer = {0};
        ULONG           FileCount  = 1;
	DIR             *ret_dir = (DIR*) malloc (sizeof(DIR));
	
	if (dd_dir->d_first) {
		dd_dir->d_first = 0;
		return dd_dir;
	}

        rc = DosFindNext((HDIR) dd_dir->d_handle, 			
                        (PVOID) &FindBuffer,
                        sizeof(FILEFINDBUF3),
                        &FileCount);

        switch (rc) {
        case ERROR_NO_MORE_FILES:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
           errno = ENOENT;
           return NULL;
           break;
        case ERROR_BUFFER_OVERFLOW:
           errno = ENOMEM;
           return NULL;
           break;
        case NO_ERROR: /* go through */
           break;
        default: 
           errno = EINVAL;
           return NULL;
          break;
        } /* endswitch */
	/* dd_dir->d_attr = (wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
                   		? 0 : wfd.dwFileAttributes; */
        /*    #define FILE_NORMAL     0x0000
              #define FILE_READONLY   0x0001
              #define FILE_HIDDEN     0x0002
              #define FILE_SYSTEM     0x0004
              #define FILE_DIRECTORY  0x0010
              #define FILE_ARCHIVED   0x0020
        */

	ret_dir->d_attr = FindBuffer.attrFile; 

	ret_dir->d_time = ret_dir->d_date = 10; 
	ret_dir->d_size         = FindBuffer.cbFile; 
	strcpy (ret_dir->d_name, FindBuffer.achName);
	return ret_dir;
}		  

int closedir (DIR *dd_dir)
{
   if (dd_dir->d_handle != 0xFFFF) {
	DosFindClose(dd_dir->d_handle);
   } /* endif */
   free (dd_dir);
   return 1;
}


