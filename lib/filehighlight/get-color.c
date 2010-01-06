/*
   File highlight plugin.
   Interface functions. get color pair index for highlighted file.

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include <string.h>


#include "../src/global.h"
#include "../src/util.h"
#include "../src/skin/skin.h"
#include "../src/filehighlight/fhl.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/*inline functions*/
inline static gboolean
mc_fhl_is_file (file_entry * fe)
{
#if S_ISREG == 0
    (void) fe;
#endif
    return S_ISREG (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_file_exec (file_entry * fe)
{
    return is_exe (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_dir (file_entry * fe)
{
#if S_ISDIR == 0
    (void) fe;
#endif
    return S_ISDIR (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_link (file_entry * fe)
{
#if S_ISLNK == 0
    (void) fe;
#endif
    return S_ISLNK (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_link_to_dir (file_entry * fe)
{
    return mc_fhl_is_link (fe) && (fe->f.link_to_dir);
}

inline static gboolean
mc_fhl_is_stale_link (file_entry * fe)
{
    return mc_fhl_is_link (fe) ? fe->f.stale_link : !mc_fhl_is_file (fe);
}

inline static gboolean
mc_fhl_is_device_char (file_entry * fe)
{
#if S_ISCHR == 0
    (void) fe;
#endif
    return S_ISCHR (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_device_block (file_entry * fe)
{
#if S_ISBLK == 0
    (void) fe;
#endif
    return S_ISBLK (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_special_socket (file_entry * fe)
{
#if S_ISSOCK == 0
    (void) fe;
#endif
    return S_ISSOCK (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_special_fifo (file_entry * fe)
{
#if S_ISFIFO == 0
    (void) fe;
#endif
    return S_ISFIFO (fe->st.st_mode);
}

inline static gboolean
mc_fhl_is_special_door (file_entry * fe)
{
#if S_ISDOOR == 0
    (void) fe;
#endif

    return S_ISDOOR (fe->st.st_mode);
}


inline static gboolean
mc_fhl_is_special (file_entry * fe)
{
    return
        (mc_fhl_is_special_socket (fe) || mc_fhl_is_special_fifo (fe) || mc_fhl_is_special_door (fe)
        );
}


/* --------------------------------------------------------------------------------------------- */

static int
mc_fhl_get_color_filetype (mc_fhl_filter_t * mc_filter, mc_fhl_t * fhl, file_entry * fe)
{
    gboolean my_color = FALSE;
    (void) fhl;

    switch (mc_filter->file_type) {
    case MC_FLHGH_FTYPE_T_FILE:
        if (mc_fhl_is_file (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_FILE_EXE:
        if ((mc_fhl_is_file (fe)) && (mc_fhl_is_file_exec (fe)))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_DIR:
        if (mc_fhl_is_dir (fe) || mc_fhl_is_link_to_dir (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_LINK_DIR:
        if (mc_fhl_is_link_to_dir (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_LINK:
        if (mc_fhl_is_link (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_HARDLINK:
        /*TODO: hanlde it */
        if (mc_fhl_is_link (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_SYMLINK:
        /*TODO: hanlde it */
        if (mc_fhl_is_link (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_STALE_LINK:
        if (mc_fhl_is_stale_link (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_DEVICE:
        if ((mc_fhl_is_device_char (fe)) || (mc_fhl_is_device_block (fe)))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_DEVICE_BLOCK:
        if (mc_fhl_is_device_block (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_DEVICE_CHAR:
        if (mc_fhl_is_device_char (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_SPECIAL:
        if (mc_fhl_is_special (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_SPECIAL_SOCKET:
        if (mc_fhl_is_special_socket (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_SPECIAL_FIFO:
        if (mc_fhl_is_special_fifo (fe))
            my_color = TRUE;
        break;
    case MC_FLHGH_FTYPE_T_SPECIAL_DOOR:
        if (mc_fhl_is_special_door (fe))
            my_color = TRUE;
        break;
    }

    return (my_color) ? mc_filter->color_pair_index : -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
mc_fhl_get_color_regexp (mc_fhl_filter_t * mc_filter, mc_fhl_t * fhl, file_entry * fe)
{
    (void) fhl;
    if (mc_filter->search_condition == NULL)
        return -1;

    if (mc_search_run (mc_filter->search_condition, fe->fname, 0, strlen (fe->fname), NULL))
        return mc_filter->color_pair_index;

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */


int
mc_fhl_get_color (mc_fhl_t * fhl, file_entry * fe)
{
    guint i;
    mc_fhl_filter_t *mc_filter;
    int ret;

    if (fhl == NULL)
        return NORMAL_COLOR;

    for (i = 0; i < fhl->filters->len; i++) {
        mc_filter = (mc_fhl_filter_t *) g_ptr_array_index (fhl->filters, i);
        switch (mc_filter->type) {
        case MC_FLHGH_T_FTYPE:
            ret = mc_fhl_get_color_filetype (mc_filter, fhl, fe);
            if (ret > 0)
                return -ret;
            break;
        case MC_FLHGH_T_EXT:
        case MC_FLHGH_T_FREGEXP:
            ret = mc_fhl_get_color_regexp (mc_filter, fhl, fe);
            if (ret > 0)
                return -ret;
            break;
        }
    }
    return NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */
