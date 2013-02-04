/** \file wtools.h
 *  \brief Header: widget based utility functions
 */

#ifndef MC__WTOOLS_H
#define MC__WTOOLS_H

/*** typedefs(not structures) and defined constants **********************************************/

/* Pass this as def_text to request a password */
#define INPUT_PASSWORD ((char *) -1)

/* Use this as header for message() - it expands to "Error" */
#define MSG_ERROR ((char *) -1)

/*** enums ***************************************************************************************/

/* flags for message() and query_dialog() */
enum
{
    D_NORMAL = 0,
    D_ERROR = (1 << 0),
    D_CENTER = (1 << 1)
} /* dialog options */ ;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* The input dialogs */
char *input_dialog (const char *header, const char *text,
                    const char *history_name, const char *def_text,
                    input_complete_t completion_flags);
char *input_dialog_help (const char *header, const char *text, const char *help,
                         const char *history_name, const char *def_text, gboolean strip_password,
                         input_complete_t completion_flags);
char *input_expand_dialog (const char *header, const char *text, const char *history_name,
                           const char *def_text, input_complete_t completion_flags);

int query_dialog (const char *header, const char *text, int flags, int count, ...);
void query_set_sel (int new_sel);

/* Create message box but don't dismiss it yet, not background safe */
struct WDialog *create_message (int flags, const char *title,
                                const char *text, ...) __attribute__ ((format (__printf__, 3, 4)));

/* Show message box, background safe */
void message (int flags, const char *title, const char *text, ...)
    __attribute__ ((format (__printf__, 3, 4)));

/*** inline functions ****************************************************************************/

#endif /* MC__WTOOLS_H */
