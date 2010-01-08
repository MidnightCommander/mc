#ifndef MC__SKIN_H
#define MC__SKIN_H

#include "src/global.h"

#include "../../lib/mcconfig/mcconfig.h"

#include "../lib/tty/color.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* Beware! When using Slang with color, not all the indexes are free.
   See color-slang.h (A_*) */

/* cache often used colors*/
#define DEFAULT_COLOR           mc_skin_color__cache[0]
#define NORMAL_COLOR            mc_skin_color__cache[1]
#define MARKED_COLOR            mc_skin_color__cache[2]
#define SELECTED_COLOR          mc_skin_color__cache[3]
#define MARKED_SELECTED_COLOR   mc_skin_color__cache[4]
#define REVERSE_COLOR           mc_skin_color__cache[5]

/* Dialog colors */
#define COLOR_NORMAL            mc_skin_color__cache[6]
#define COLOR_FOCUS             mc_skin_color__cache[7]
#define COLOR_HOT_NORMAL        mc_skin_color__cache[8]
#define COLOR_HOT_FOCUS         mc_skin_color__cache[9]

/* Error dialog colors */
#define ERROR_COLOR             mc_skin_color__cache[10]
#define ERROR_HOT_NORMAL        mc_skin_color__cache[11]
#define ERROR_HOT_FOCUS         mc_skin_color__cache[12]

/* Menu colors */
#define MENU_ENTRY_COLOR        mc_skin_color__cache[13]
#define MENU_SELECTED_COLOR     mc_skin_color__cache[14]
#define MENU_HOT_COLOR          mc_skin_color__cache[15]
#define MENU_HOTSEL_COLOR       mc_skin_color__cache[16]

/*
 * This should be selectable independently. Default has to be black background
 * foreground does not matter at all.
 */
#define GAUGE_COLOR             mc_skin_color__cache[17]
#define INPUT_COLOR             mc_skin_color__cache[18]

#define HELP_NORMAL_COLOR       mc_skin_color__cache[19]
#define HELP_ITALIC_COLOR       mc_skin_color__cache[20]
#define HELP_BOLD_COLOR         mc_skin_color__cache[21]
#define HELP_LINK_COLOR         mc_skin_color__cache[22]
#define HELP_SLINK_COLOR        mc_skin_color__cache[23]

#define VIEW_UNDERLINED_COLOR   mc_skin_color__cache[24]

/*
 * editor colors - only 4 for normal, search->found, select, and whitespace
 * respectively
 * Last is defined to view color.
 */
#define EDITOR_NORMAL_COLOR     mc_skin_color__cache[25]
#define EDITOR_BOLD_COLOR       mc_skin_color__cache[26]
#define EDITOR_MARKED_COLOR     mc_skin_color__cache[27]
#define EDITOR_WHITESPACE_COLOR mc_skin_color__cache[28]
/* color of left 8 char status per line */
#define LINE_STATE_COLOR        mc_skin_color__cache[29]
#define BOOK_MARK_COLOR         mc_skin_color__cache[30]
#define BOOK_MARK_FOUND_COLOR   mc_skin_color__cache[31]

#define BUTTONBAR_HOTKEY_COLOR   mc_skin_color__cache[32]
#define BUTTONBAR_BUTTON_COLOR   mc_skin_color__cache[33]

#define MC_SKIN_COLOR_CACHE_COUNT 34


/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_skin_struct {
    gchar *name;
    gchar *description;
    mc_config_t *config;
    GHashTable *colors;
} mc_skin_t;

/*** global variables defined in .c file *********************************************************/

extern int mc_skin_color__cache[];
extern mc_skin_t mc_skin__default;

/*** declarations of public functions ************************************************************/

gboolean mc_skin_init (GError **);
void mc_skin_deinit (void);

int mc_skin_color_get (const gchar *, const gchar *);

void mc_skin_lines_parse_ini_file (mc_skin_t *);

gchar *mc_skin_get(const gchar *, const gchar *, const gchar *);


#endif
