#ifndef __COLOR_H
#define __COLOR_H

void init_colors (void);

extern int hascolors;
extern int use_colors;
extern int disable_colors;

extern int attr_pairs [];
#ifdef HAVE_SLANG
#   define MY_COLOR_PAIR(x) COLOR_PAIR(x)
#else
#   define MY_COLOR_PAIR(x) (COLOR_PAIR(x) | attr_pairs [x])
#endif

#define BEST_COLOR(co,bw) (use_colors ? MY_COLOR_PAIR(co) : bw)

/* Beware! When using Slang with color, not all the indexes are free.
   See myslang.h (A_*) */
#define NORMAL_COLOR          BEST_COLOR (1, 0)
#define SELECTED_COLOR        BEST_COLOR (2, A_REVERSE)
#define MARKED_COLOR          BEST_COLOR (3, A_BOLD)

#ifdef HAVE_SLANG
#define MARKED_SELECTED_COLOR BEST_COLOR (4, (SLtt_Use_Ansi_Colors ? A_BOLD_REVERSE : A_REVERSE | A_BOLD))
#else
#define MARKED_SELECTED_COLOR BEST_COLOR (4, A_REVERSE | A_BOLD)
#endif

#define ERROR_COLOR           BEST_COLOR (5, 0)
#define MENU_ENTRY_COLOR      BEST_COLOR (6, A_REVERSE)
#define REVERSE_COLOR         BEST_COLOR (7, A_REVERSE)
#define Q_SELECTED_COLOR      BEST_COLOR (2, 0)
#define Q_UNSELECTED_COLOR    REVERSE_COLOR

extern int sel_mark_color  [4];
extern int dialog_colors   [4];

/* Dialog colors */			   
#define COLOR_NORMAL       BEST_COLOR (8, A_REVERSE)
#define COLOR_FOCUS        BEST_COLOR (9, A_BOLD)
#define COLOR_HOT_NORMAL   BEST_COLOR (10, 0)
#define COLOR_HOT_FOCUS    BEST_COLOR (11, 0)
			   
#define VIEW_UNDERLINED_COLOR BEST_COLOR (12, A_UNDERLINE)
#define MENU_SELECTED_COLOR   BEST_COLOR (13, A_BOLD)
#define MENU_HOT_COLOR        BEST_COLOR (14, 0)
#define MENU_HOTSEL_COLOR     BEST_COLOR (15, 0)

#define HELP_NORMAL_COLOR  BEST_COLOR (16, A_REVERSE)
#define HELP_ITALIC_COLOR  BEST_COLOR (17, A_REVERSE)
#define HELP_BOLD_COLOR    BEST_COLOR (18, A_REVERSE)
#define HELP_LINK_COLOR    BEST_COLOR (19, 0)
#define HELP_SLINK_COLOR   BEST_COLOR (20, A_BOLD)

/*
 * This should be selectable independently. Default has to be black background
 * foreground does not matter at all.
 */
#define GAUGE_COLOR        BEST_COLOR (21, 0)
#define INPUT_COLOR        BEST_COLOR (22, 0)

/* Add this to color panel, on BW all pairs are normal */
#define DIRECTORY_COLOR    BEST_COLOR (23, 0)
#define EXECUTABLE_COLOR   BEST_COLOR (24, 0)
#define LINK_COLOR         BEST_COLOR (25, 0)
#define STALLED_LINK_COLOR BEST_COLOR (26, A_UNDERLINE)
#define DEVICE_COLOR       BEST_COLOR (27, 0)
#define SPECIAL_COLOR      BEST_COLOR (28, 0)
#define CORE_COLOR         BEST_COLOR (29, 0)


/* For the default color any unused index may be chosen. */
#define DEFAULT_COLOR_INDEX   30
#define DEFAULT_COLOR      BEST_COLOR (DEFAULT_COLOR_INDEX, 0)

/*
 * editor colors - only 3 for normal, search->found, and select, respectively
 * Last is defined to view color.
 */
#define EDITOR_NORMAL_COLOR_INDEX    34
#define EDITOR_NORMAL_COLOR          BEST_COLOR (EDITOR_NORMAL_COLOR_INDEX, 0)
#define EDITOR_BOLD_COLOR            BEST_COLOR (35, A_BOLD)
#define EDITOR_MARKED_COLOR          BEST_COLOR (36, A_REVERSE)
#define EDITOR_UNDERLINED_COLOR      VIEW_UNDERLINED_COLOR

#ifdef HAVE_SLANG
#   define CTYPE char *
#else
#   define CTYPE int
#endif

void mc_init_pair (int index, CTYPE foreground, CTYPE background);
int try_alloc_color_pair (char *fg, char *bg);
void dealloc_color_pairs (void);

#endif /* __COLOR_H */
