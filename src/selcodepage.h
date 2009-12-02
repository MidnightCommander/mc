
/** \file selcodepage.h
 *  \brief Header: user %interface for charset %selection
 */

#ifndef MC_SELCODEPAGE_H
#define MC_SELCODEPAGE_H

#ifdef HAVE_CHARSET

#include "global.h"

int select_charset (int center_y, int center_x, int current_charset, gboolean seldisplay);
gboolean do_select_codepage (void);

/* some results of select_charset() */
#define SELECT_CHARSET_CANCEL		-2
/* select_charset() returns this value if dialog has been canceled */
#define SELECT_CHARSET_OTHER_8BIT	-1
/* select_charset() returns this value if seldisplay == TRUE
 * and the last item has been selected. Last item is "Other 8 bits" */
#define SELECT_CHARSET_NO_TRANSLATE	-1
/* select_charset() returns this value if seldisplay == FALSE
 * and the 1st item has been selected. 1st item is "No translation" */
/* In other cases select_charset() returns non-negative value
 * which is number of codepage in codepage list */

#endif				/* HAVE_CHARSET */

#endif				/* MC_SELCODEPAGE_H */
