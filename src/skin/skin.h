#ifndef MC__SKIN_H
#define MC__SKIN_H

#include "../../src/mcconfig/mcconfig.h"

#include "../src/tty/color.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* Beware! When using Slang with color, not all the indexes are free.
   See color-slang.h (A_*) */

/* cache often used colors*/
#define DEFAULT_COLOR           mc_skin_color__cache[0]
#define NORMAL_COLOR            mc_skin_color__cache[1]
#define MARKED_COLOR            mc_skin_color__cache[2]
#define SELECTED_COLOR          mc_skin_color__cache[3]
#define REVERSE_COLOR           mc_skin_color__cache[4]


#define MARKED_SELECTED_COLOR   mc_skin_color_get("core", "markselect")
#define ERROR_COLOR             mc_skin_color_get("error", "_default_")
#define MENU_ENTRY_COLOR        mc_skin_color_get("menu", "_default_")

/* Dialog colors */
#define COLOR_NORMAL            mc_skin_color_get("dialog", "_default_")
#define COLOR_FOCUS             mc_skin_color_get("dialog", "focus")
#define COLOR_HOT_NORMAL        mc_skin_color_get("dialog", "hotnormal")
#define COLOR_HOT_FOCUS         mc_skin_color_get("dialog", "hotfocus")

#define VIEW_UNDERLINED_COLOR   mc_skin_color_get("viewer", "underline")
#define MENU_SELECTED_COLOR     mc_skin_color_get("menu", "selected")
#define MENU_HOT_COLOR          mc_skin_color_get("menu", "hot")
#define MENU_HOTSEL_COLOR       mc_skin_color_get("menu", "hotselected")

#define HELP_NORMAL_COLOR       mc_skin_color_get("help", "_default_")
#define HELP_ITALIC_COLOR       mc_skin_color_get("help", "italic")
#define HELP_BOLD_COLOR         mc_skin_color_get("help", "bold")
#define HELP_LINK_COLOR         mc_skin_color_get("help", "link")
#define HELP_SLINK_COLOR        mc_skin_color_get("help", "slink")

/*
 * This should be selectable independently. Default has to be black background
 * foreground does not matter at all.
 */
#define GAUGE_COLOR             mc_skin_color_get("core", "gauge")
#define INPUT_COLOR             mc_skin_color_get("core", "input")


/*
 * editor colors - only 4 for normal, search->found, select, and whitespace
 * respectively
 * Last is defined to view color.
 */
#define EDITOR_NORMAL_COLOR     mc_skin_color_get("editor", "_default_")
#define EDITOR_BOLD_COLOR       mc_skin_color_get("editor", "bold")
#define EDITOR_MARKED_COLOR     mc_skin_color_get("editor", "marked")
#define EDITOR_WHITESPACE_COLOR mc_skin_color_get("editor", "whitespace")

/* color of left 8 char status per line */
#define LINE_STATE_COLOR        mc_skin_color_get("editor", "linestate")

/* Error dialog colors */
#define ERROR_HOT_NORMAL        mc_skin_color_get("error", "hotnormal")
#define ERROR_HOT_FOCUS         mc_skin_color_get("error", "hotfocus")



/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_skin_struct{
    gchar *name;
    gchar *description;

    mc_config_t *config;

    GHashTable *colors;

} mc_skin_t;

/*** global variables defined in .c file *********************************************************/

extern int mc_skin_color__cache[];

/*** declarations of public functions ************************************************************/

void mc_skin_init(void);
void mc_skin_deinit(void);

int mc_skin_color_get(const gchar *, const gchar *);


#endif
