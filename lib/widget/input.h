
/** \file input.h
 *  \brief Header: WInput widget
 */

#ifndef MC__WIDGET_INPUT_H
#define MC__WIDGET_INPUT_H

/*** typedefs(not structures) and defined constants **********************************************/

/* For history load-save functions */
#define INPUT_LAST_TEXT ((char *) 2)

/*** enums ***************************************************************************************/

typedef enum
{
    WINPUTC_MAIN,               /* color used */
    WINPUTC_MARK,               /* color for marked text */
    WINPUTC_UNCHANGED,          /* color for inactive text (Is first keystroke) */
    WINPUTC_HISTORY,            /* color for history list */
    WINPUTC_COUNT_COLORS        /* count of used colors */
} input_colors_enum_t;

/* completion flags */
typedef enum
{
    INPUT_COMPLETE_FILENAMES = 1 << 0,
    INPUT_COMPLETE_HOSTNAMES = 1 << 1,
    INPUT_COMPLETE_COMMANDS = 1 << 2,
    INPUT_COMPLETE_VARIABLES = 1 << 3,
    INPUT_COMPLETE_USERNAMES = 1 << 4,
    INPUT_COMPLETE_CD = 1 << 5,
    INPUT_COMPLETE_SHELL_ESC = 1 << 6,

    INPUT_COMPLETE_DEFAULT = INPUT_COMPLETE_FILENAMES
        | INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_VARIABLES | INPUT_COMPLETE_USERNAMES
} input_complete_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef int input_colors_t[WINPUTC_COUNT_COLORS];

typedef struct
{
    Widget widget;
    int point;                  /* cursor position in the input line in characters */
    int mark;                   /* the mark position in characters */
    gboolean highlight;         /* there is a selected block */
    int term_first_shown;       /* column of the first shown character */
    size_t current_max_size;    /* maximum length of input line (bytes) */
    int field_width;            /* width of the editing field */
    input_colors_t color;
    gboolean first;             /* is first keystroke? */
    int disable_update;         /* do we want to skip updates? */
    gboolean is_password;       /* is this a password input line? */
    char *buffer;               /* pointer to editing buffer */
    GList *history;             /* the history */
    gboolean need_push;         /* need to push the current Input on hist? */
    char **completions;         /* possible completions array */
    input_complete_t completion_flags;
    char *history_name;         /* name of history for loading and saving */
    char charbuf[MB_LEN_MAX];   /* buffer for multibytes characters */
    size_t charpoint;           /* point to end of mulibyte sequence in charbuf */
} WInput;

/*** global variables defined in .c file *********************************************************/

extern int quote;

/*** declarations of public functions ************************************************************/

WInput *input_new (int y, int x, const int *input_colors,
                   int len, const char *text, const char *histname,
                   input_complete_t completion_flags);
/* callbac is public; needed for command line */
cb_ret_t input_callback (Widget * w, widget_msg_t msg, int parm);
const int *input_get_default_colors (void);
void input_set_origin (WInput * i, int x, int field_width);
cb_ret_t input_handle_char (WInput * in, int key);
int input_key_is_in_map (WInput * in, int key);
void input_assign_text (WInput * in, const char *text);
void input_insert (WInput * in, const char *text, gboolean insert_extra_space);
void input_set_point (WInput * in, int pos);
void input_update (WInput * in, gboolean clear_first);
void input_enable_update (WInput * in);
void input_disable_update (WInput * in);
void input_clean (WInput * in);
void input_free_completions (WInput * in);

/* src/complete.c */
void complete (WInput * in);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_INPUT_H */
