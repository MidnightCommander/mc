/*
 * Tree Store
 *
 * Contains a storage of the file system tree representation
 *
   Copyright (C) 1994, 1995, 1996, 1997 The Free Software Foundation

   Written: 1994, 1996 Janne Kukonlehto
            1997 Norbert Warmuth
            1996, 1999 Miguel de Icaza
	    
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   This module has been converted to be a widget.

   The program load and saves the tree each time the tree widget is
   created and destroyed.  This is required for the future vfs layer,
   it will be possible to have tree views over virtual file systems.
*/

#include <config.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "treestore.h"
#include "profile.h"
#include "setup.h"

#define TREE_SIGNATURE "Midnight Commander TreeStore v 2.0"

static struct TreeStore ts;

static tree_entry *tree_store_add_entry(const char *name);

static void
tree_store_dirty(int state)
{
    ts.dirty = state;
}

/* Returns number of common characters */
static int
str_common(const char *s1, const char *s2)
{
    int result = 0;

    while (*s1++ == *s2++)
	result++;
    return result;
}

/* The directory names are arranged in a single linked list in the same
   order as they are displayed. When the tree is displayed the expected
   order is like this:
        /
        /bin
        /etc
        /etc/X11
        /etc/rc.d
        /etc.old/X11
        /etc.old/rc.d
        /usr

   i.e. the required collating sequence when comparing two directory names is 
        '\0' < PATH_SEP < all-other-characters-in-encoding-order

    Since strcmp doesn't fulfil this requirement we use pathcmp when
    inserting directory names into the list. The meaning of the return value 
    of pathcmp and strcmp are the same (an integer less than, equal to, or 
    greater than zero if p1 is found to be less than, to match, or be greater 
    than p2.
 */
static int
pathcmp(const char *p1, const char *p2)
{
    for (; *p1 == *p2; p1++, p2++)
	if (*p1 == '\0')
	    return 0;

    if (*p1 == '\0')
	return -1;
    if (*p2 == '\0')
	return 1;
    if (*p1 == PATH_SEP)
	return -1;
    if (*p2 == PATH_SEP)
	return 1;
    return (*p1 - *p2);
}

/* Searches for specified directory */
tree_entry *
tree_store_whereis(const char *name)
{
    tree_entry *current = ts.tree_first;
    int flag = -1;

    while (current && (flag = pathcmp(current->name, name)) < 0)
	current = current->next;

    if (flag == 0)
	return current;
    else
	return NULL;
}

struct TreeStore *
tree_store_get(void)
{
    return &ts;
}

static char *
decode(char *buffer)
{
    char *res = g_strdup(buffer);
    char *p, *q;

    for (p = q = res; *p; p++, q++) {
	if (*p == '\n') {
	    *q = 0;
	    return res;
	}

	if (*p != '\\') {
	    *q = *p;
	    continue;
	}

	p++;

	switch (*p) {
	case 'n':
	    *q = '\n';
	    break;
	case '\\':
	    *q = '\\';
	    break;
	}
    }
    *q = *p;

    return res;
}

/* Loads the tree store from the specified filename */
static int
tree_store_load_from(char *name)
{
    FILE *file;
    char buffer[MC_MAXPATHLEN + 20], oldname[MC_MAXPATHLEN];
    char *different;
    int len, common;
    int do_load;

    g_return_val_if_fail(name != NULL, FALSE);

    if (ts.loaded)
	return TRUE;

    file = fopen(name, "r");

    if (file) {
	fgets(buffer, sizeof(buffer), file);

	if (strncmp(buffer, TREE_SIGNATURE, strlen(TREE_SIGNATURE)) != 0) {
	    fclose(file);
	    do_load = FALSE;
	} else
	    do_load = TRUE;
    } else
	do_load = FALSE;

    if (do_load) {
	ts.loaded = TRUE;

	/* File open -> read contents */
	oldname[0] = 0;
	while (fgets(buffer, MC_MAXPATHLEN, file)) {
	    tree_entry *e;
	    int scanned;
	    char *name;

	    /* Skip invalid records */
	    if ((buffer[0] != '0' && buffer[0] != '1'))
		continue;

	    if (buffer[1] != ':')
		continue;

	    scanned = buffer[0] == '1';

	    name = decode(buffer + 2);

	    len = strlen(name);
	    if (name[0] != PATH_SEP) {
		/* Clear-text decompression */
		char *s = strtok(name, " ");

		if (s) {
		    common = atoi(s);
		    different = strtok(NULL, "");
		    if (different) {
			strcpy(oldname + common, different);
			if (vfs_file_is_local(oldname)) {
			    e = tree_store_add_entry(oldname);
			    e->scanned = scanned;
			}
		    }
		}
	    } else {
		if (vfs_file_is_local(name)) {
		    e = tree_store_add_entry(name);
		    e->scanned = scanned;
		}
		strcpy(oldname, name);
	    }
	    g_free(name);
	}
	fclose(file);
    }

    /* Nothing loaded, we add some standard directories */
    if (!ts.tree_first) {
	tree_store_add_entry(PATH_SEP_STR);
	tree_store_rescan(PATH_SEP_STR);
	ts.loaded = TRUE;
    }

    return TRUE;
}

/**
 * tree_store_load:
 * @void: 
 * 
 * Loads the tree from the default location.
 * 
 * Return value: TRUE if success, FALSE otherwise.
 **/
int
tree_store_load(void)
{
    char *name;
    int retval;

    name = concat_dir_and_file(home_dir, MC_TREE);
    retval = tree_store_load_from(name);
    g_free(name);

    return retval;
}

static char *
encode(const char *string)
{
    int special_chars;
    const char *p;
    char *q;
    char *res;

    for (special_chars = 0, p = string; *p; p++) {
	if (*p == '\n' || *p == '\\')
	    special_chars++;
    }

    res = g_malloc(p - string + special_chars + 1);
    for (p = string, q = res; *p; p++, q++) {
	if (*p != '\n' && *p != '\\') {
	    *q = *p;
	    continue;
	}

	*q++ = '\\';

	switch (*p) {
	case '\n':
	    *q = 'n';
	    break;

	case '\\':
	    *q = '\\';
	    break;
	}
    }
    *q = 0;
    return res;
}

/* Saves the tree to the specified filename */
static int
tree_store_save_to(char *name)
{
    tree_entry *current;
    FILE *file;

    file = fopen(name, "w");
    if (!file)
	return errno;

    fprintf(file, "%s\n", TREE_SIGNATURE);

    current = ts.tree_first;
    while (current) {
	int i, common;

	if (vfs_file_is_local(current->name)) {
	    /* Clear-text compression */
	    if (current->prev
		&& (common =
		    str_common(current->prev->name, current->name)) > 2) {
		char *encoded = encode(current->name + common);

		i = fprintf(file, "%d:%d %s\n", current->scanned, common,
			    encoded);
		g_free(encoded);
	    } else {
		char *encoded = encode(current->name);

		i = fprintf(file, "%d:%s\n", current->scanned, encoded);
		g_free(encoded);
	    }

	    if (i == EOF) {
		fprintf(stderr, _("Cannot write to the %s file:\n%s\n"),
			name, unix_error_string(errno));
		break;
	    }
	}
	current = current->next;
    }
    tree_store_dirty(FALSE);
    fclose(file);

    return 0;
}

/**
 * tree_store_save:
 * @void: 
 * 
 * Saves the tree to the default file in an atomic fashion.
 * 
 * Return value: 0 if success, errno on error.
 **/
int
tree_store_save(void)
{
    char *tmp;
    char *name;
    int retval;

    tmp = concat_dir_and_file(home_dir, MC_TREE_TMP);
    retval = tree_store_save_to(tmp);

    if (retval) {
	g_free(tmp);
	return retval;
    }

    name = concat_dir_and_file(home_dir, MC_TREE);
    retval = rename(tmp, name);

    g_free(tmp);
    g_free(name);

    if (retval)
	return errno;
    else
	return 0;
}

static tree_entry *
tree_store_add_entry(const char *name)
{
    int flag = -1;
    tree_entry *current = ts.tree_first;
    tree_entry *old = NULL;
    tree_entry *new;
    int i, len;
    int submask = 0;

    if (ts.tree_last && ts.tree_last->next)
	abort();

    /* Search for the correct place */
    while (current && (flag = pathcmp(current->name, name)) < 0) {
	old = current;
	current = current->next;
    }

    if (flag == 0)
	return current;		/* Already in the list */

    /* Not in the list -> add it */
    new = g_new0(tree_entry, 1);
    if (!current) {
	/* Append to the end of the list */
	if (!ts.tree_first) {
	    /* Empty list */
	    ts.tree_first = new;
	    new->prev = NULL;
	} else {
	    old->next = new;
	    new->prev = old;
	}
	new->next = NULL;
	ts.tree_last = new;
    } else {
	/* Insert in to the middle of the list */
	new->prev = old;
	if (old) {
	    /* Yes, in the middle */
	    new->next = old->next;
	    old->next = new;
	} else {
	    /* Nope, in the beginning of the list */
	    new->next = ts.tree_first;
	    ts.tree_first = new;
	}
	new->next->prev = new;
    }

    /* Calculate attributes */
    new->name = g_strdup(name);
    len = strlen(new->name);
    new->sublevel = 0;
    for (i = 0; i < len; i++)
	if (new->name[i] == PATH_SEP) {
	    new->sublevel++;
	    new->subname = new->name + i + 1;
	}
    if (new->next)
	submask = new->next->submask;
    else
	submask = 0;
    submask |= 1 << new->sublevel;
    submask &= (2 << new->sublevel) - 1;
    new->submask = submask;
    new->mark = 0;

    /* Correct the submasks of the previous entries */
    current = new->prev;
    while (current && current->sublevel > new->sublevel) {
	current->submask |= 1 << new->sublevel;
	current = current->prev;
    }

    /* The entry has now been added */

    if (new->sublevel > 1) {
	/* Let's check if the parent directory is in the tree */
	char *parent = g_strdup(new->name);
	int i;

	for (i = strlen(parent) - 1; i > 1; i--) {
	    if (parent[i] == PATH_SEP) {
		parent[i] = 0;
		tree_store_add_entry(parent);
		break;
	    }
	}
	g_free(parent);
    }

    tree_store_dirty(TRUE);
    return new;
}

static Hook *remove_entry_hooks;

void
tree_store_add_entry_remove_hook(tree_store_remove_fn callback, void *data)
{
    add_hook(&remove_entry_hooks, (void (*)(void *)) callback, data);
}

void
tree_store_remove_entry_remove_hook(tree_store_remove_fn callback)
{
    delete_hook(&remove_entry_hooks, (void (*)(void *)) callback);
}

static void
tree_store_notify_remove(tree_entry * entry)
{
    Hook *p = remove_entry_hooks;
    tree_store_remove_fn r;

    while (p) {
	r = (tree_store_remove_fn) p->hook_fn;
	r(entry, p->hook_data);
	p = p->next;
    }
}

static tree_entry *
remove_entry(tree_entry * entry)
{
    tree_entry *current = entry->prev;
    long submask = 0;
    tree_entry *ret = NULL;

    tree_store_notify_remove(entry);

    /* Correct the submasks of the previous entries */
    if (entry->next)
	submask = entry->next->submask;
    while (current && current->sublevel > entry->sublevel) {
	submask |= 1 << current->sublevel;
	submask &= (2 << current->sublevel) - 1;
	current->submask = submask;
	current = current->prev;
    }

    /* Unlink the entry from the list */
    if (entry->prev)
	entry->prev->next = entry->next;
    else
	ts.tree_first = entry->next;

    if (entry->next)
	entry->next->prev = entry->prev;
    else
	ts.tree_last = entry->prev;

    /* Free the memory used by the entry */
    g_free(entry->name);
    g_free(entry);

    return ret;
}

void
tree_store_remove_entry(const char *name)
{
    tree_entry *current, *base, *old;
    int len;

    g_return_if_fail(name != NULL);

    /* Miguel Ugly hack */
    if (name[0] == PATH_SEP && name[1] == 0)
	return;
    /* Miguel Ugly hack end */

    base = tree_store_whereis(name);
    if (!base)
	return;			/* Doesn't exist */

    len = strlen(base->name);
    current = base->next;
    while (current
	   && strncmp(current->name, base->name, len) == 0
	   && (current->name[len] == '\0'
	       || current->name[len] == PATH_SEP)) {
	old = current;
	current = current->next;
	remove_entry(old);
    }
    remove_entry(base);
    tree_store_dirty(TRUE);

    return;
}

/* This subdirectory exists -> clear deletion mark */
void
tree_store_mark_checked(const char *subname)
{
    char *name;
    tree_entry *current, *base;
    int flag = 1, len;
    if (!ts.loaded)
	return;

    if (ts.check_name == NULL)
	return;

    /* Calculate the full name of the subdirectory */
    if (subname[0] == '.' &&
	(subname[1] == 0 || (subname[1] == '.' && subname[2] == 0)))
	return;
    if (ts.check_name[0] == PATH_SEP && ts.check_name[1] == 0)
	name = g_strconcat(PATH_SEP_STR, subname, NULL);
    else
	name = concat_dir_and_file(ts.check_name, subname);

    /* Search for the subdirectory */
    current = ts.check_start;
    while (current && (flag = pathcmp(current->name, name)) < 0)
	current = current->next;

    if (flag != 0) {
	/* Doesn't exist -> add it */
	current = tree_store_add_entry(name);
	ts.add_queue = g_list_prepend(ts.add_queue, g_strdup(name));
    }
    g_free(name);

    /* Clear the deletion mark from the subdirectory and its children */
    base = current;
    if (base) {
	len = strlen(base->name);
	base->mark = 0;
	current = base->next;
	while (current
	       && strncmp(current->name, base->name, len) == 0
	       && (current->name[len] == '\0'
		   || current->name[len] == PATH_SEP || len == 1)) {
	    current->mark = 0;
	    current = current->next;
	}
    }
}

/* Mark the subdirectories of the current directory for delete */
tree_entry *
tree_store_start_check(const char *path)
{
    tree_entry *current, *retval;
    int len;

    if (!ts.loaded)
	return NULL;

    g_return_val_if_fail(ts.check_name == NULL, NULL);
    ts.check_start = NULL;

    /* Search for the start of subdirectories */
    current = tree_store_whereis(path);
    if (!current) {
	struct stat s;

	if (mc_stat(path, &s) == -1)
	    return NULL;

	if (!S_ISDIR(s.st_mode))
	    return NULL;

	current = tree_store_add_entry(path);
	ts.check_name = g_strdup(path);

	return current;
    }

    ts.check_name = g_strdup(path);

    retval = current;

    /* Mark old subdirectories for delete */
    ts.check_start = current->next;
    len = strlen(ts.check_name);

    current = ts.check_start;
    while (current
	   && strncmp(current->name, ts.check_name, len) == 0
	   && (current->name[len] == '\0' || current->name[len] == PATH_SEP
	       || len == 1)) {
	current->mark = 1;
	current = current->next;
    }

    return retval;
}

/* Delete subdirectories which still have the deletion mark */
void
tree_store_end_check(void)
{
    tree_entry *current, *old;
    int len;
    GList *the_queue, *l;

    if (!ts.loaded)
	return;

    g_return_if_fail(ts.check_name != NULL);

    /* Check delete marks and delete if found */
    len = strlen(ts.check_name);

    current = ts.check_start;
    while (current
	   && strncmp(current->name, ts.check_name, len) == 0
	   && (current->name[len] == '\0' || current->name[len] == PATH_SEP
	       || len == 1)) {
	old = current;
	current = current->next;
	if (old->mark)
	    remove_entry(old);
    }

    /* get the stuff in the scan order */
    ts.add_queue = g_list_reverse(ts.add_queue);
    the_queue = ts.add_queue;
    ts.add_queue = NULL;
    g_free(ts.check_name);
    ts.check_name = NULL;

    for (l = the_queue; l; l = l->next) {
	g_free(l->data);
    }

    g_list_free(the_queue);
}

static void
process_special_dirs(GList ** special_dirs, char *file)
{
    char *token;
    char *buffer = g_malloc(4096);
    char *s;

    GetPrivateProfileString("Special dirs", "list",
			    "", buffer, 4096, file);
    s = buffer;
    while ((token = strtok(s, ",")) != NULL) {
	*special_dirs = g_list_prepend(*special_dirs, g_strdup(token));
	s = NULL;
    }
    g_free(buffer);
}

static gboolean
should_skip_directory(char *dir)
{
    static GList *special_dirs;
    GList *l;
    static int loaded;

    if (loaded == 0) {
	loaded = 1;
	setup_init();
	process_special_dirs(&special_dirs, profile_name);
	process_special_dirs(&special_dirs, global_profile_name);
    }

    for (l = special_dirs; l; l = l->next) {
	if (strncmp(dir, l->data, strlen(l->data)) == 0)
	    return TRUE;
    }
    return FALSE;
}

tree_entry *
tree_store_rescan(const char *dir)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat buf;
    tree_entry *entry;

    if (should_skip_directory(dir)) {
	entry = tree_store_add_entry(dir);
	entry->scanned = 1;

	return entry;
    }

    entry = tree_store_start_check(dir);

    if (!entry)
	return NULL;

    dirp = mc_opendir(dir);
    if (dirp) {
	for (dp = mc_readdir(dirp); dp; dp = mc_readdir(dirp)) {
	    char *full_name;

	    if (dp->d_name[0] == '.') {
		if (dp->d_name[1] == 0
		    || (dp->d_name[1] == '.' && dp->d_name[2] == 0))
		    continue;
	    }

	    full_name = concat_dir_and_file(dir, dp->d_name);
	    if (mc_lstat(full_name, &buf) != -1) {
		if (S_ISDIR(buf.st_mode))
		    tree_store_mark_checked(dp->d_name);
	    }
	    g_free(full_name);
	}
	mc_closedir(dirp);
    }
    tree_store_end_check();
    entry->scanned = 1;

    return entry;
}
