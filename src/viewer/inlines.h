#ifndef MC__VIEWER_INLINES_H
#define MC__VIEWER_INLINES_H

#include <limits.h>             /* CHAR_BIT */

/*** typedefs(not structures) and defined constants **********************************************/

#define OFF_T_BITWIDTH  ((unsigned int) (sizeof (off_t) * CHAR_BIT - 1))
#define OFFSETTYPE_MAX (((off_t) 1 << (OFF_T_BITWIDTH - 1)) - 1)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

/* difference or zero */
static inline off_t
mcview_offset_doz (off_t a, off_t b)
{
    return (a >= b) ? a - b : 0;
}

/* --------------------------------------------------------------------------------------------- */

static inline off_t
mcview_offset_rounddown (off_t a, off_t b)
{
    g_assert (b != 0);
    return a - a % b;
}

/* --------------------------------------------------------------------------------------------- */

/* difference or zero */
static inline screen_dimen
mcview_dimen_doz (screen_dimen a, screen_dimen b)
{
    return (a >= b) ? a - b : 0;
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Simple Primitive Functions for WView }}} */
static inline gboolean
mcview_is_in_panel (WView * view)
{
    return (view->dpy_frame_size != 0);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_may_still_grow (WView * view)
{
    return (view->growbuf_in_use && !view->growbuf_finished);
}

/* --------------------------------------------------------------------------------------------- */

/* returns TRUE if the idx lies in the half-open interval
 * [offset; offset + size), FALSE otherwise.
 */
static inline gboolean
mcview_already_loaded (off_t offset, off_t idx, size_t size)
{
    return (offset <= idx && idx - offset < (off_t) size);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte_file (WView * view, off_t byte_index, int *retval)
{
    g_assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
    {
        if (retval)
            *retval = view->ds_file_data[byte_index - view->ds_file_offset];
        return TRUE;
    }
    if (retval)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte (WView * view, off_t offset, int *retval)
{
    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        return mcview_get_byte_growing_buffer (view, offset, retval);
    case DS_FILE:
        return mcview_get_byte_file (view, offset, retval);
    case DS_STRING:
        return mcview_get_byte_string (view, offset, retval);
    case DS_NONE:
        return mcview_get_byte_none (view, offset, retval);
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte_indexed (WView * view, off_t base, off_t ofs, int *retval)
{
    if (base <= OFFSETTYPE_MAX - ofs)
    {
        return mcview_get_byte (view, base + ofs, retval);
    }
    if (retval)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_count_backspaces (WView * view, off_t offset)
{
    int backspaces = 0;
    int c;
    while (offset >= 2 * backspaces && mcview_get_byte (view, offset - 2 * backspaces, &c)
           && c == '\b')
        backspaces++;
    return backspaces;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_is_nroff_sequence (WView * view, off_t offset)
{
    int c0, c1, c2;

    /* The following commands are ordered to speed up the calculation. */

    if (!mcview_get_byte_indexed (view, offset, 1, &c1) || c1 != '\b')
        return FALSE;

    if (!mcview_get_byte_indexed (view, offset, 0, &c0) || !g_ascii_isprint (c0))
        return FALSE;

    if (!mcview_get_byte_indexed (view, offset, 2, &c2) || !g_ascii_isprint (c2))
        return FALSE;

    return (c0 == c2 || c0 == '_' || (c0 == '+' && c2 == 'o'));
}

/* --------------------------------------------------------------------------------------------- */

static inline void
mcview_growbuf_read_all_data (WView * view)
{
    mcview_growbuf_read_until (view, OFFSETTYPE_MAX);
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__VIEWER_INLINES_H */
