/* The Memory Allocation Debugging system
   Copyright (C) 1994 Janne Kukonlehto.

   To use MAD define HAVE_MAD and include "mad.h" in all the *.c files.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include "mad.h"

#undef tempnam

#undef malloc
#undef calloc
#undef realloc
#undef xmalloc
#undef strdup
#undef strndup
#undef free

#undef g_malloc
#undef g_malloc0
#undef g_calloc
#undef g_realloc
#undef g_strdup
#undef g_strndup
#undef g_free

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>	/* For kill() */
#ifdef HAVE_UNISTD_H
#   include <unistd.h>	/* For getpid() */
#endif
#include <glib.h>

/* Here to avoid non empty translation units */
#ifdef HAVE_MAD

/* Maximum number of memory area handles,
   increase this if you run out of handles */
#define MAD_MAX_AREAS 20000 /* vfs leaks Lots of memory*/
/* Maximum file name length */
#define MAD_MAX_FILE 50
/* Signature for detecting overwrites */
#define MAD_SIGNATURE (('M'<<24)|('a'<<16)|('d'<<8)|('S'))

typedef struct {
    int in_use;
    long *start_sig;
    char file [MAD_MAX_FILE];
    int line;
    void *data;
    long *end_sig;
} mad_mem_area;

static mad_mem_area mem_areas [MAD_MAX_AREAS];
static FILE *memlog;

#define MAD_CHECK_CALL_FACTOR 30        /* Perform actual test every N call. */
static int Mad_check_call_delay;
static int Alloc_idx_hint = 0;
static struct /*mad_stats_struct*/ {
	int last_max_i;
	long check_call_cnt;
} Stats = { -1, 0 } ;

void *watch_free_pointer = 0;

void
mad_init (void)
{
    memlog = stderr;
}

void mad_set_debug (const char *file)
{
    if((memlog=fopen (file, "w+")) == NULL)
	memlog = stderr;
}

/* This function is only called by the mad_check function */
static void mad_abort (char *message, int area, char *file, int line)
{
    fprintf (memlog, "MAD: %s in area %d.\r\n", message, area);
    fprintf (memlog, "     Allocated in file \"%s\" at line %d.\r\n",
	     mem_areas [area].file, mem_areas [area].line);
    fprintf (memlog, "     Discovered in file \"%s\" at line %d.\r\n",
	     file, line);
    fprintf (memlog, "MAD: Core dumping...\r\n");
    fflush (memlog);
    kill (getpid (), 3);
}

/* Code repeated in lots of places. Could be merged with mad_abort() above. */
static void mad_fatal_error(const char *problem, void *ptr, const char *file, int line)
{
    if (NULL != ptr) {
        fprintf(memlog, "MAD: %s: %p.\r\n", problem, ptr);
    } else {
        fprintf(memlog, "MAD: %s.\r\n", problem);
    }
    fprintf(memlog, "     Discovered in file \"%s\" at line %d.\r\n", file, line);
    fprintf(memlog, "MAD: Aborting...\r\n");
    fflush(memlog); 
    abort();
}

/* Checks all the allocated memory areas.
   This is called everytime memory is allocated or freed.
   You can also call it anytime you think memory might be corrupted. */
void mad_check (char *file, int line)
{
    int i;

    if (--Mad_check_call_delay > 0)
	return;
    Mad_check_call_delay = MAD_CHECK_CALL_FACTOR;

    for (i = 0; i < MAD_MAX_AREAS; i++){
 	if (! mem_areas [i].in_use)
	    continue;
	if (*(mem_areas [i].start_sig) != MAD_SIGNATURE)
	    mad_abort ("Overwrite error: Bad start signature", i, file, line);
	if (*(mem_areas [i].end_sig) != MAD_SIGNATURE)
	    mad_abort ("Overwrite error: Bad end signature", i, file, line);
    }
    Stats.check_call_cnt++;
    fflush (memlog);
}

/* Allocates a memory area. Used instead of malloc and calloc. */
void *mad_alloc (int size, char *file, int line)
{
    int i;
    char *area;

    mad_check (file, line);

    for (i = Alloc_idx_hint; i < MAD_MAX_AREAS; i++) {
        if (! mem_areas [i].in_use)
            break;
    }
    if (i >= MAD_MAX_AREAS)
	mad_fatal_error("Out of memory area handles. Increase the value of MAD_MAX_AREAS.", NULL, file, line);

    Alloc_idx_hint = i+1;
    if (i > Stats.last_max_i)
        Stats.last_max_i = i;

    mem_areas [i].in_use = 1;
    size = (size + 3) & (~3); /* Alignment */
    area = (char*) g_malloc (size + 2 * sizeof (long));
    if (!area)
	mad_fatal_error("Out of memory.", NULL, file, line);

    mem_areas [i].start_sig = (long*) area;
    mem_areas [i].data = (area + sizeof (long));
    mem_areas [i].end_sig = (long*) (area + size + sizeof (long));
    *(mem_areas [i].start_sig) = MAD_SIGNATURE;
    *(mem_areas [i].end_sig) = MAD_SIGNATURE;

    if (strlen (file) >= MAD_MAX_FILE)
	file [MAD_MAX_FILE - 1] = 0;
    strcpy (mem_areas [i].file, file);
    mem_areas [i].line = line;

    return mem_areas [i].data;
}

/* Reallocates a memory area. Used instead of realloc. */
void *mad_realloc (void *ptr, int newsize, char *file, int line)
{
    int i;
    char *area;

    if (!ptr)
        return (mad_alloc (newsize, file, line));

    mad_check (file, line);

    for (i = 0; i < MAD_MAX_AREAS; i++){
 	if (! mem_areas [i].in_use)
	    continue;
	if (mem_areas [i].data == ptr)
	    break;
    }
    if (i >= MAD_MAX_AREAS)
	mad_fatal_error("Attempted to realloc unallocated pointer", ptr, file, line);

    newsize = (newsize + 3) & (~3); /* Alignment */
    area = (char*) g_realloc (mem_areas [i].start_sig, newsize + 2 * sizeof (long));
    if (!area)
	mad_fatal_error("Out of memory", NULL, file, line);

    /* Reuses a position in mem_areas[] and thus does not set .in_use */
    mem_areas [i].start_sig = (long*) area;
    mem_areas [i].data = (area + sizeof (long));
    mem_areas [i].end_sig = (long*) (area + newsize + sizeof (long));
    *(mem_areas [i].start_sig) = MAD_SIGNATURE;
    *(mem_areas [i].end_sig) = MAD_SIGNATURE;

    if (strlen (file) >= MAD_MAX_FILE)
	file [MAD_MAX_FILE - 1] = 0;
    strcpy (mem_areas [i].file, file);
    mem_areas [i].line = line;

    return mem_areas [i].data;
}

/* Allocates a memory area. Used instead of malloc and calloc. */
void *mad_alloc0 (int size, char *file, int line)
{
    char *t;

    t = (char *) mad_alloc (size, file, line);
    memset (t, 0, size);
    return (void *) t;
}

/* Duplicates a character string. Used instead of strdup. */
char *mad_strdup (const char *s, char *file, int line)
{
    char *t;

    t = (char *) mad_alloc (strlen (s) + 1, file, line);
    strcpy (t, s);
    return t;
}

/* Duplicates a character string. Used instead of strndup. */
/* Dup of GLib's gstrfuncs.c:g_strndup() */
char *mad_strndup (const char *s, int n, char *file, int line)
{
    if(s) {
        char *new_str = mad_alloc(n + 1, file, line);
        strncpy(new_str, s, n);
        new_str[n] = '\0';
        return new_str;
    } else {
        return NULL;
    }
}

/* Frees a memory area. Used instead of free. */
void mad_free (void *ptr, char *file, int line)
{
    int i;

    mad_check (file, line);

    if (watch_free_pointer && ptr == watch_free_pointer){
	fprintf (memlog, "MAD: Watch free pointer found in file \"%s\" at line %d.\r\n",
		 file, line);
	fflush (memlog);
    }

    if (ptr == NULL){
	fprintf (memlog, "MAD: Attempted to free a NULL pointer in file \"%s\" at line %d.\r\n",
		 file, line);
	fflush (memlog);
	return;
    }

    /* Do a quick search in the neighborhood of Alloc_idx_hint. */
    for ( i = MAX(Alloc_idx_hint-100, 0); i <= Alloc_idx_hint; i++ )
	if ( mem_areas [i].data == ptr )
	    goto found_it;

    for ( i = MIN(Alloc_idx_hint+100, MAD_MAX_AREAS-1); i > Alloc_idx_hint; i-- )
	if ( mem_areas [i].data == ptr )
	    goto found_it;

    for (i = 0; i < MAD_MAX_AREAS; i++)
	if ( mem_areas [i].data == ptr )
	    goto found_it;
	
    mad_fatal_error("Attempted to free an unallocated pointer", ptr, file, line);

  found_it:
    g_free (mem_areas [i].start_sig);
    mem_areas [i].in_use = 0;

    mem_areas[i].data = NULL;   /* Kill the pointer - no need to check .in_use above.*/
    if ( i < Alloc_idx_hint )
        Alloc_idx_hint = i;
}

char *mad_tempnam (char *a, char *b)
{
    char *t, *u;
    t = tempnam(a,b); /* This malloc's internal buffer.. */
    u = mad_strdup(t, "(mad_tempnam)", 0);
    free(t);
    return u;
}

/* Outputs a list of unfreed memory areas,
   to be called as a last thing before exiting */
void mad_finalize (char *file, int line)
{
    int i;

    Mad_check_call_delay = 0;
    mad_check (file, line);

    /* Following can be commented out if you don't want to see the
       memory leaks of the Midnight Commander */
#if 1
    for (i = 0; i < MAD_MAX_AREAS; i++){
 	if (! mem_areas [i].in_use)
	    continue;
	fprintf (memlog, "MAD: Unfreed pointer: %p.\r\n", mem_areas [i].data);
	fprintf (memlog, "     Allocated in file \"%s\" at line %d.\r\n",
		 mem_areas [i].file, mem_areas [i].line);
	fprintf (memlog, "     Discovered in file \"%s\" at line %d.\r\n",
		 file, line);
	fflush (memlog);
    }
    fprintf(memlog,
        "MAD: Stats -\n     last_max_i:%d\n     check_call_cnt:%ld\n",
	Stats.last_max_i, Stats.check_call_cnt
    );
#endif
}

char *
mad_strconcat (const char *first, ...)
{
    va_list ap;
    long len;
    char *data, *result;

    if (!first)
	return 0;

    len = strlen (first) + 1;
    va_start (ap, first);

    while ((data = va_arg (ap, char *)) != 0)
	len += strlen (data);

    result = mad_alloc(len, "(mad_strconcat)", 0);

    va_end (ap);

    va_start (ap, first);
    strcpy (result, first);

    while ((data = va_arg (ap, char *)) != 0)
	strcat (result, data);

    va_end (ap);

    return result;
}


/* These two functions grabbed from GLib's gstrfuncs.c */
char*
mad_strdup_vprintf (const char *format, va_list args1)
{
  char *buffer;
  va_list args2;

  G_VA_COPY (args2, args1);

  buffer = mad_alloc(g_printf_string_upper_bound(format, args1), "(mad_strdup_vprintf)", 0);

  vsprintf (buffer, format, args2);
  va_end (args2);

  return buffer;
}

char*
mad_strdup_printf (const char *format, ...)
{
  char *buffer;
  va_list args;

  va_start (args, format);
  buffer = mad_strdup_vprintf(format, args);
  va_end (args);

  return buffer;
}
#endif /* HAVE_MAD */
