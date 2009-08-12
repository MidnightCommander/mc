#ifndef MC_VIEWER_INLINES_H
#define MC_VIEWER_INLINES_H

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

/*** inline functions ****************************************************************************/

static inline off_t
mcview_offset_doz (off_t a, off_t b)
{
    return (a >= b) ? a - b : 0;
}

/* --------------------------------------------------------------------------------------------- */

static inline off_t
mcview_offset_rounddown (off_t a, off_t b)
{
    assert (b != 0);
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

static inline screen_dimen
mcview_dimen_min (screen_dimen a, screen_dimen b)
{
    return (a < b) ? a : b;
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Simple Primitive Functions for mcview_t }}} */
static inline gboolean
mcview_is_in_panel (mcview_t * view)
{
    return (view->dpy_frame_size != 0);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_may_still_grow (mcview_t * view)
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
    return (offset <= idx && idx - offset < size);
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_get_byte_file (mcview_t * view, off_t byte_index)
{
    assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return view->ds_file_data[byte_index - view->ds_file_offset];
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_get_byte (mcview_t * view, off_t offset)
{
    switch (view->datasource) {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        return mcview_get_byte_growing_buffer (view, offset);
    case DS_FILE:
        return mcview_get_byte_file (view, offset);
    case DS_STRING:
        return mcview_get_byte_string (view, offset);
    case DS_NONE:
        return mcview_get_byte_none (view, offset);
    }
    assert (!"Unknown datasource type");
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_get_byte_indexed (mcview_t * view, off_t base, off_t ofs)
{
    if (base <= OFFSETTYPE_MAX - ofs)
        return mcview_get_byte (view, base + ofs);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_count_backspaces (mcview_t * view, off_t offset)
{
    int backspaces = 0;
    while (offset >= 2 * backspaces && mcview_get_byte (view, offset - 2 * backspaces) == '\b')
        backspaces++;
    return backspaces;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_is_nroff_sequence (mcview_t * view, off_t offset)
{
    int c0, c1, c2;

    /* The following commands are ordered to speed up the calculation. */

    c1 = mcview_get_byte_indexed (view, offset, 1);
    if (c1 == -1 || c1 != '\b')
        return FALSE;

    c0 = mcview_get_byte_indexed (view, offset, 0);
    if (c0 == -1 || !g_ascii_isprint (c0))
        return FALSE;

    c2 = mcview_get_byte_indexed (view, offset, 2);
    if (c2 == -1 || !g_ascii_isprint (c2))
        return FALSE;

    return (c0 == c2 || c0 == '_' || (c0 == '+' && c2 == 'o'));
}

/* --------------------------------------------------------------------------------------------- */

#endif
