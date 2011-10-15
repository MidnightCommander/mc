/*
   Virtual File System path handlers

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * \brief Source: Virtual File System: path handlers
 * \author Slava Zanko
 * \date 2011
 */


#include <config.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* concat_dir_and_file */
#include "lib/serialize.h"

#include "vfs.h"
#include "utilvfs.h"
#include "xdirentry.h"
#include "path.h"

extern GPtrArray *vfs__classes_list;

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
path_magic (const char *path)
{
    struct stat buf;

    return (stat (path, &buf) != 0);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Splits path extracting vfs part.
 *
 * Splits path
 * \verbatim /p1#op/inpath \endverbatim
 * into
 * \verbatim inpath,op; \endverbatim
 * returns which vfs it is.
 * What is left in path is p1. You still want to g_free(path), you DON'T
 * want to free neither *inpath nor *op
 */

static struct vfs_class *
_vfs_split_with_semi_skip_count (char *path, const char **inpath, const char **op,
                                 size_t skip_count)
{
    char *semi;
    char *slash;
    struct vfs_class *ret;

    if (path == NULL)
        vfs_die ("Cannot split NULL");

    semi = strrstr_skip_count (path, "#", skip_count);

    if ((semi == NULL) || (!path_magic (path)))
        return NULL;

    slash = strchr (semi, PATH_SEP);
    *semi = '\0';

    if (op != NULL)
        *op = NULL;

    if (inpath != NULL)
        *inpath = NULL;

    if (slash != NULL)
        *slash = '\0';

    ret = vfs_prefix_to_class (semi + 1);
    if (ret != NULL)
    {
        if (op != NULL)
            *op = semi + 1;
        if (inpath != NULL)
            *inpath = slash != NULL ? slash + 1 : NULL;
        return ret;
    }

    if (slash != NULL)
        *slash = PATH_SEP;

    *semi = '#';
    ret = _vfs_split_with_semi_skip_count (path, inpath, op, skip_count + 1);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * remove //, /./ and /../
 *
 * @return newly allocated string
 */

static char *
vfs_canon (const char *path)
{
    if (!path)
        vfs_die ("Cannot canonicalize NULL");

    /* Relative to current directory */
    if (*path != PATH_SEP)
    {
        char *local, *result, *curr_dir;

        curr_dir = vfs_get_current_dir ();
        local = concat_dir_and_file (curr_dir, path);
        g_free (curr_dir);

        result = vfs_canon (local);
        g_free (local);
        return result;
    }

    /*
     * So we have path of following form:
     * /p1/p2#op/.././././p3#op/p4. Good luck.
     */
    {
        char *result = g_strdup (path);
        canonicalize_pathname (result);
        return result;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Build URL parameters (such as user:pass@host:port) from one path element object
 *
 * @param element path element
 *
 * @return newly allocated string
 */

static char *
vfs_path_build_url_params_str (vfs_path_element_t * element)
{
    GString *buffer;

    if (element == NULL)
        return NULL;

    buffer = g_string_new ("");

    if (element->user != NULL)
        g_string_append (buffer, element->user);

    if (element->password != NULL)
    {
        g_string_append_c (buffer, ':');
        g_string_append (buffer, element->password);
    }

    if (element->host != NULL)
    {
        if ((element->user != NULL) || (element->password != NULL))
            g_string_append_c (buffer, '@');
        if (element->ipv6)
            g_string_append_c (buffer, '[');
        g_string_append (buffer, element->host);
        if (element->ipv6)
            g_string_append_c (buffer, ']');
    }

    if ((element->port) != 0 && (element->host != NULL))
    {
        g_string_append_c (buffer, ':');
        g_string_append_printf (buffer, "%d", element->port);
    }

    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/** get encoding after last #enc: or NULL, if part does not contain #enc:
 *
 * @param path string
 *
 * @return newly allocated string.
 */

static char *
vfs_get_encoding (const char *path)
{
    char result[16];
    char *work;
    char *semi;
    char *slash;
    work = g_strdup (path);

    /* try found #enc: */
    semi = g_strrstr (work, VFS_ENCODING_PREFIX);

    if (semi != NULL && (semi == work || *(semi - 1) == PATH_SEP))
    {
        semi += strlen (VFS_ENCODING_PREFIX);   /* skip "#enc:" */
        slash = strchr (semi, PATH_SEP);
        if (slash != NULL)
            slash[0] = '\0';

        g_strlcpy (result, semi, sizeof (result));
        g_free (work);
        return g_strdup (result);
    }
    else
    {
        g_free (work);
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**  Extract the hostname and username from the path
 *
 * Format of the path is [user@]hostname:port/remote-dir, e.g.:
 *
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * ftp://joe:password@foo.se
 *
 * @param path_element is an input string to be parsed
 * @param path is an input string to be parsed
 *
 * @return g_malloc()ed url info.
 *         If the user is empty, e.g. ftp://@roxanne/private, and URL_USE_ANONYMOUS
 *         is not set, then the current login name is supplied.
 *         Return value is a g_malloc()ed structure with the pathname relative to the
 *         host.
 */

static void
vfs_path_url_split (vfs_path_element_t * path_element, const char *path)
{
    char *pcopy;
    const char *pend;
    char *dir, *colon, *inner_colon, *at, *rest;

    path_element->port = 0;

    pcopy = g_strdup (path);
    pend = pcopy + strlen (pcopy);
    dir = pcopy;

    /* search for any possible user */
    at = strrchr (pcopy, '@');

    /* We have a username */
    if (at == NULL)
        rest = pcopy;
    else
    {
        *at = '\0';
        inner_colon = strchr (pcopy, ':');
        if (inner_colon != NULL)
        {
            *inner_colon = '\0';
            inner_colon++;
            path_element->password = g_strdup (inner_colon);
        }

        if (*pcopy != '\0')
            path_element->user = g_strdup (pcopy);

        if (pend == at + 1)
            rest = at;
        else
            rest = at + 1;
    }

    /* Check if the host comes with a port spec, if so, chop it */
    if (*rest != '[')
        colon = strchr (rest, ':');
    else
    {
        colon = strchr (++rest, ']');
        if (colon != NULL)
        {
            colon[0] = '\0';
            colon[1] = '\0';
            colon++;
            path_element->ipv6 = TRUE;
        }
    }

    if (colon != NULL)
    {
        *colon = '\0';
        if (sscanf (colon + 1, "%d", &path_element->port) == 1)
        {
            if (path_element->port <= 0 || path_element->port >= 65536)
                path_element->port = 0;
        }
        else
            while (*(++colon) != '\0')
            {
                switch (*colon)
                {
                case 'C':
                    path_element->port = 1;
                    break;
                case 'r':
                    path_element->port = 2;
                    break;
                }
            }
    }
    path_element->host = g_strdup (rest);
    g_free (pcopy);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * get VFS class for the given name
 *
 * @param class_name name of class
 *
 * @return pointer to class structure or NULL if class not found
 */

static struct vfs_class *
vfs_get_class_by_name (const char *class_name)
{
    guint i;

    if (class_name == NULL)
        return NULL;

    for (i = 0; i < vfs__classes_list->len; i++)
    {
        struct vfs_class *vfs = (struct vfs_class *) g_ptr_array_index (vfs__classes_list, i);
        if ((vfs->name != NULL) && (strcmp (vfs->name, class_name) == 0))
            return vfs;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check if path string contain URL-like elements
 *
 * @param path_str path
 *
 * @return TRUE if path is deprecated or FALSE otherwise
 */

static gboolean
vfs_path_is_str_path_deprecated (const char *path_str)
{
    return strstr (path_str, VFS_PATH_URL_DELIMITER) == NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Split path string to path elements by deprecated algorithm.
 *
 * @param path_str VFS-path
 *
 * @return pointer to newly created vfs_path_t object with filled path elements array.
*/

static vfs_path_t *
vfs_path_from_str_deprecated_parser (char *path)
{
    vfs_path_t *vpath;
    vfs_path_element_t *element;
    struct vfs_class *class;
    const char *local, *op;

    vpath = vfs_path_new ();

    while ((class = _vfs_split_with_semi_skip_count (path, &local, &op, 0)) != NULL)
    {
        char *url_params;
        element = g_new0 (vfs_path_element_t, 1);
        element->class = class;
        if (local == NULL)
            local = "";
        element->path = vfs_translate_path_n (local);

        element->encoding = vfs_get_encoding (local);
        element->dir.converter =
            (element->encoding != NULL) ? str_crt_conv_from (element->encoding) : INVALID_CONV;

        url_params = strchr (op, ':');  /* skip VFS prefix */
        if (url_params != NULL)
        {
            *url_params = '\0';
            url_params++;
            vfs_path_url_split (element, url_params);
        }

        if (*op != '\0')
            element->vfs_prefix = g_strdup (op);

        vpath->path = g_list_prepend (vpath->path, element);
    }
    if (path[0] != '\0')
    {
        element = g_new0 (vfs_path_element_t, 1);
        element->class = g_ptr_array_index (vfs__classes_list, 0);
        element->path = vfs_translate_path_n (path);

        element->encoding = vfs_get_encoding (path);
        element->dir.converter =
            (element->encoding != NULL) ? str_crt_conv_from (element->encoding) : INVALID_CONV;
        vpath->path = g_list_prepend (vpath->path, element);
    }

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/** Split path string to path elements by URL algorithm.
 *
 * @param path_str VFS-path
 *
 * @return pointer to newly created vfs_path_t object with filled path elements array.
*/

static vfs_path_t *
vfs_path_from_str_uri_parser (char *path)
{
    vfs_path_t *vpath;
    vfs_path_element_t *element;

    char *url_delimiter;

    vpath = vfs_path_new ();

    while ((url_delimiter = g_strrstr (path, VFS_PATH_URL_DELIMITER)) != NULL)
    {
        char *vfs_prefix_start;
        char *real_vfs_prefix_start = url_delimiter;
        char *slash_pointer;
        struct vfs_s_subclass *sub = NULL;

        while (real_vfs_prefix_start > path && *(real_vfs_prefix_start) != PATH_SEP)
            real_vfs_prefix_start--;
        vfs_prefix_start = real_vfs_prefix_start;

        if (*(vfs_prefix_start) == PATH_SEP)
            vfs_prefix_start += 1;

        *url_delimiter = '\0';

        element = g_new0 (vfs_path_element_t, 1);
        element->class = vfs_prefix_to_class (vfs_prefix_start);
        element->vfs_prefix = g_strdup (vfs_prefix_start);

        url_delimiter += strlen (VFS_PATH_URL_DELIMITER);
        sub = VFSDATA (element);
        if (sub != NULL && sub->flags & VFS_S_REMOTE)
        {
            slash_pointer = strchr (url_delimiter, PATH_SEP);
            if (slash_pointer == NULL)
            {
                element->path = g_strdup ("");
            }
            else
            {
                element->path = vfs_translate_path_n (slash_pointer + 1);
                element->encoding = vfs_get_encoding (slash_pointer);

                *slash_pointer = '\0';
            }
            vfs_path_url_split (element, url_delimiter);
        }
        else
        {
            element->path = vfs_translate_path_n (url_delimiter);
            element->encoding = vfs_get_encoding (url_delimiter);
        }
        element->dir.converter =
            (element->encoding != NULL) ? str_crt_conv_from (element->encoding) : INVALID_CONV;
        vpath->path = g_list_prepend (vpath->path, element);

        if (real_vfs_prefix_start > path && *(real_vfs_prefix_start) == PATH_SEP)
            *real_vfs_prefix_start = '\0';
        else
            *(real_vfs_prefix_start + 1) = '\0';
    }

    if (path[0] != '\0')
    {
        element = g_new0 (vfs_path_element_t, 1);
        element->class = g_ptr_array_index (vfs__classes_list, 0);
        element->path = vfs_translate_path_n (path);
        element->encoding = vfs_get_encoding (path);
        element->dir.converter =
            (element->encoding != NULL) ? str_crt_conv_from (element->encoding) : INVALID_CONV;
        vpath->path = g_list_prepend (vpath->path, element);
    }

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Convert first elements_count elements from vfs_path_t to string representation.
 *
 * @param vpath pointer to vfs_path_t object
 * @param elements_count count of first elements for convert
 * @param flags flags for parser
 *
 * @return pointer to newly created string.
 */

#define vfs_append_from_path(appendfrom) \
{ \
    if ((*appendfrom != PATH_SEP) && (*appendfrom != '\0') \
        && (buffer->str[buffer->len - 1] != PATH_SEP)) \
        g_string_append_c (buffer, PATH_SEP); \
    g_string_append (buffer, appendfrom); \
}

char *
vfs_path_to_str_elements_count (const vfs_path_t * vpath, int elements_count)
{
    int element_index;
    GString *buffer;
    GString *recode_buffer;

    if (vpath == NULL)
        return NULL;

    if (elements_count > vfs_path_elements_count (vpath))
        elements_count = vfs_path_elements_count (vpath);

    if (elements_count < 0)
        elements_count = vfs_path_elements_count (vpath) + elements_count;

    buffer = g_string_new ("");
    recode_buffer = g_string_new ("");

    for (element_index = 0; element_index < elements_count; element_index++)
    {
        vfs_path_element_t *element = vfs_path_get_by_index (vpath, element_index);

        if (element->vfs_prefix != NULL)
        {
            char *url_str;

            if (buffer->str[buffer->len - 1] != '/')
                g_string_append_c (buffer, '/');

            g_string_append (buffer, element->vfs_prefix);
            g_string_append (buffer, VFS_PATH_URL_DELIMITER);

            url_str = vfs_path_build_url_params_str (element);
            if (*url_str != '\0')
                g_string_append (buffer, url_str);

            g_free (url_str);
        }

        if (vfs_path_element_need_cleanup_converter (element))
        {
            if (buffer->str[buffer->len - 1] != PATH_SEP)
                g_string_append (buffer, PATH_SEP_STR);
            g_string_append (buffer, VFS_ENCODING_PREFIX);
            g_string_append (buffer, element->encoding);
            str_vfs_convert_from (element->dir.converter, element->path, recode_buffer);
            vfs_append_from_path (recode_buffer->str);
            g_string_set_size (recode_buffer, 0);
        }
        else
        {
            vfs_append_from_path (element->path);
        }
    }
    g_string_free (recode_buffer, TRUE);
    return g_string_free (buffer, FALSE);
}

#undef vfs_append_from_path

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert vfs_path_t to string representation.
 *
 * @param vpath pointer to vfs_path_t object
 *
 * @return pointer to newly created string.
 */

char *
vfs_path_to_str (const vfs_path_t * vpath)
{
    return vfs_path_to_str_elements_count (vpath, vfs_path_elements_count (vpath));
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Split path string to path elements with flags for change parce process.
 *
 * @param path_str VFS-path
 * @param flags flags for parser
 *
 * @return pointer to newly created vfs_path_t object with filled path elements array.
 */

vfs_path_t *
vfs_path_from_str_flags (const char *path_str, vfs_path_flag_t flags)
{
    vfs_path_t *vpath;
    char *path;

    if (path_str == NULL)
        return NULL;

    if ((flags & VPF_NO_CANON) == 0)
        path = vfs_canon (path_str);
    else
        path = g_strdup (path_str);

    if (path == NULL)
        return NULL;

    if ((flags & VPF_USE_DEPRECATED_PARSER) != 0 && vfs_path_is_str_path_deprecated (path))
        vpath = vfs_path_from_str_deprecated_parser (path);
    else
        vpath = vfs_path_from_str_uri_parser (path);

    g_free (path);

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Split path string to path elements.
 *
 * @param path_str VFS-path
 *
 * @return pointer to newly created vfs_path_t object with filled path elements array.
 */

vfs_path_t *
vfs_path_from_str (const char *path_str)
{
    return vfs_path_from_str_flags (path_str, VPF_NONE);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Create new vfs_path_t object.
 *
 * @return pointer to newly created vfs_path_t object.
 */

vfs_path_t *
vfs_path_new (void)
{
    vfs_path_t *vpath;
    vpath = g_new0 (vfs_path_t, 1);
    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Get count of path elements.
 *
 * @param vpath pointer to vfs_path_t object
 *
 * @return count of path elements.
 */

int
vfs_path_elements_count (const vfs_path_t * vpath)
{
    return (vpath != NULL && vpath->path != NULL) ? g_list_length (vpath->path) : 0;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Get one path element by index.
 *
 * @param vpath pointer to vfs_path_t object
 * @param element_index element index. May have negative value (in this case count was started at the end of list).
 *
 * @return path element.
 */

vfs_path_element_t *
vfs_path_get_by_index (const vfs_path_t * vpath, int element_index)
{
    if (element_index < 0)
        element_index += vfs_path_elements_count (vpath);

    if (element_index < 0)
        vfs_die ("vfs_path_get_by_index: incorrect index!");

    return g_list_nth_data (vpath->path, element_index);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Clone one path element
 *
 * @param element pointer to vfs_path_element_t object
 *
 * @return Newly allocated path element
 */

vfs_path_element_t *
vfs_path_element_clone (const vfs_path_element_t * element)
{
    vfs_path_element_t *new_element = g_new0 (vfs_path_element_t, 1);
    memcpy (new_element, element, sizeof (vfs_path_element_t));

    new_element->user = g_strdup (element->user);
    new_element->password = g_strdup (element->password);
    new_element->host = g_strdup (element->host);
    new_element->path = g_strdup (element->path);
    new_element->encoding = g_strdup (element->encoding);
    if (vfs_path_element_need_cleanup_converter (element) && new_element->encoding != NULL)
        new_element->dir.converter = str_crt_conv_from (new_element->encoding);
    new_element->vfs_prefix = g_strdup (element->vfs_prefix);

    return new_element;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Free one path element.
 *
 * @param element pointer to vfs_path_element_t object
 *
 */

void
vfs_path_element_free (vfs_path_element_t * element)
{
    if (element == NULL)
        return;

    g_free (element->user);
    g_free (element->password);
    g_free (element->host);
    g_free (element->path);
    g_free (element->encoding);
    g_free (element->vfs_prefix);

    if (vfs_path_element_need_cleanup_converter (element))
    {
        str_close_conv (element->dir.converter);
    }

    g_free (element);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Clone path
 *
 * @param vpath pointer to vfs_path_t object
 *
 * @return Newly allocated path object
 */

vfs_path_t *
vfs_path_clone (const vfs_path_t * vpath)
{
    vfs_path_t *new_vpath;
    int vpath_element_index;
    if (vpath == NULL)
        return NULL;

    new_vpath = vfs_path_new ();
    for (vpath_element_index = 0; vpath_element_index < vfs_path_elements_count (vpath);
         vpath_element_index++)
    {
        new_vpath->path =
            g_list_append (new_vpath->path,
                           vfs_path_element_clone (vfs_path_get_by_index
                                                   (vpath, vpath_element_index)));
    }

    return new_vpath;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Free vfs_path_t object.
 *
 * @param vpath pointer to vfs_path_t object
 *
 */

void
vfs_path_free (vfs_path_t * path)
{
    if (path == NULL)
        return;
    g_list_foreach (path->path, (GFunc) vfs_path_element_free, NULL);
    g_list_free (path->path);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Remove one path element by index
 *
 * @param vpath pointer to vfs_path_t object
 * @param element_index element index. May have negative value (in this case count was started at the end of list).
 *
 */

void
vfs_path_remove_element_by_index (vfs_path_t * vpath, int element_index)
{
    vfs_path_element_t *element;

    if ((vpath == NULL) || (vfs_path_elements_count (vpath) == 1))
        return;

    if (element_index < 0)
        element_index = vfs_path_elements_count (vpath) + element_index;

    element = g_list_nth_data (vpath->path, element_index);
    vpath->path = g_list_remove (vpath->path, element);
    vfs_path_element_free (element);
}

/* --------------------------------------------------------------------------------------------- */
/** Return VFS class for the given prefix */

struct vfs_class *
vfs_prefix_to_class (const char *prefix)
{
    guint i;

    /* Avoid first class (localfs) that would accept any prefix */
    for (i = 1; i < vfs__classes_list->len; i++)
    {
        struct vfs_class *vfs = (struct vfs_class *) g_ptr_array_index (vfs__classes_list, i);
        if (vfs->which != NULL)
        {
            if (vfs->which (vfs, prefix) == -1)
                continue;
            return vfs;
        }

        if (vfs->prefix != NULL && strncmp (prefix, vfs->prefix, strlen (vfs->prefix)) == 0)
            return vfs;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check if need cleanup charset converter for vfs_path_element_t
 *
 * @param element part of path
 *
 * @return TRUE if need cleanup converter or FALSE otherwise
 */

gboolean
vfs_path_element_need_cleanup_converter (const vfs_path_element_t * element)
{
    return (element->dir.converter != str_cnv_from_term && element->dir.converter != INVALID_CONV);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Serialize vfs_path_t object to string
 *
 * @param vpath data for serialization
 * @param error contain pointer to object for handle error code and message
 *
 * @return serialized vpath as newly allocated string
 */

char *
vfs_path_serialize (const vfs_path_t * vpath, GError ** error)
{
    mc_config_t *cpath = mc_config_init (NULL);
    ssize_t element_index;
    char *ret_value;

    if ((vpath == NULL) || (vfs_path_elements_count (vpath) == 0))
    {
        g_set_error (error, MC_ERROR, -1, "vpath object is empty");
        return NULL;

    }
    for (element_index = 0; element_index < vfs_path_elements_count (vpath); element_index++)
    {
        char *groupname = g_strdup_printf ("path-element-%zd", element_index);
        vfs_path_element_t *element = vfs_path_get_by_index (vpath, element_index);

        /* convert one element to config group */

        mc_config_set_string_raw (cpath, groupname, "path", element->path);
        mc_config_set_string_raw (cpath, groupname, "class-name", element->class->name);
        mc_config_set_string_raw (cpath, groupname, "encoding", element->encoding);

        mc_config_set_string_raw (cpath, groupname, "vfs_prefix", element->vfs_prefix);

        mc_config_set_string_raw (cpath, groupname, "user", element->user);
        mc_config_set_string_raw (cpath, groupname, "password", element->password);
        mc_config_set_string_raw (cpath, groupname, "host", element->host);
        if (element->port != 0)
            mc_config_set_int (cpath, groupname, "port", element->port);

        g_free (groupname);
    }

    ret_value = mc_serialize_config (cpath, error);
    mc_config_deinit (cpath);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Deserialize string to vfs_path_t object
 *
 * @param data data for serialization
 * @param error contain pointer to object for handle error code and message
 *
 * @return newly allocated vfs_path_t object
 */

vfs_path_t *
vfs_path_deserialize (const char *data, GError ** error)
{
    mc_config_t *cpath = mc_deserialize_config (data, error);
    size_t element_index = 0;
    vfs_path_t *vpath;

    if (cpath == NULL)
        return NULL;

    vpath = vfs_path_new ();

    while (TRUE)
    {
        vfs_path_element_t *element;
        char *cfg_value;
        char *groupname;

        groupname = g_strdup_printf ("path-element-%zd", element_index);
        if (!mc_config_has_group (cpath, groupname))
        {
            g_free (groupname);
            break;
        }

        element = g_new0 (vfs_path_element_t, 1);

        cfg_value = mc_config_get_string_raw (cpath, groupname, "class-name", NULL);
        element->class = vfs_get_class_by_name (cfg_value);
        if (element->class == NULL)
        {
            g_free (element);
            vfs_path_free (vpath);
            g_set_error (error, MC_ERROR, -1, "Unable to find VFS class by name '%s'", cfg_value);
            g_free (cfg_value);
            mc_config_deinit (cpath);
            return NULL;
        }
        g_free (cfg_value);

        element->path = mc_config_get_string_raw (cpath, groupname, "path", NULL);
        element->encoding = mc_config_get_string_raw (cpath, groupname, "encoding", NULL);
        element->dir.converter =
            (element->encoding != NULL) ? str_crt_conv_from (element->encoding) : INVALID_CONV;

        element->vfs_prefix = mc_config_get_string_raw (cpath, groupname, "vfs_prefix", NULL);

        element->user = mc_config_get_string_raw (cpath, groupname, "user", NULL);
        element->password = mc_config_get_string_raw (cpath, groupname, "password", NULL);
        element->host = mc_config_get_string_raw (cpath, groupname, "host", NULL);
        element->port = mc_config_get_int (cpath, groupname, "port", 0);

        vpath->path = g_list_append (vpath->path, element);

        g_free (groupname);
        element_index++;
    }

    mc_config_deinit (cpath);
    if (vfs_path_elements_count (vpath) == 0)
    {
        vfs_path_free (vpath);
        g_set_error (error, MC_ERROR, -1, "No any path elements found");
        return NULL;
    }

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */
