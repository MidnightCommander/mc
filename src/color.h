#ifndef __COLOR_H
#define __COLOR_H

void init_colors (void);
void toggle_color_mode (void);
void configure_colors_string (char *color_string);

extern int hascolors;
extern int use_colors;
extern int disable_colors;

extern int attr_pairs [];
#ifdef HAVE_GNOME
#   define MY_COLOR_PAIR(x) x
#   define PORT_COLOR(co,bw) co
#else
#   ifdef HAVE_SLANG
#       define MY_COLOR_PAIR(x) COLOR_PAIR(x)
#   else
#       define MY_COLOR_PAIR(x) (COLOR_PAIR(x) | attr_pairs [x])
#   endif
#define PORT_COLOR(co,bw) (use_colors?co:bw)
#endif

/* Beware! When using Slang with color, not all the indexes are free.
   See myslang.h (A_*) */
#define NORMAL_COLOR          (PORT_COLOR (MY_COLOR_PAIR (1), 0))
#define SELECTED_COLOR        (PORT_COLOR (MY_COLOR_PAIR (2),A_REVERSE))
#define MARKED_COLOR          (PORT_COLOR (MY_COLOR_PAIR (3),A_BOLD))

#ifdef HAVE_SLANG
#define MARKED_SELECTED_COLOR (PORT_COLOR (MY_COLOR_PAIR (4),(SLtt_Use_Ansi_Colors ? A_BOLD_REVERSE : A_REVERSE | A_BOLD)))
#else
#define MARKED_SELECTED_COLOR (PORT_COLOR (MY_COLOR_PAIR (4),A_REVERSE | A_BOLD))
#endif

#define ERROR_COLOR           (PORT_COLOR (MY_COLOR_PAIR (5),0))
#define MENU_ENTRY_COLOR      (PORT_COLOR (MY_COLOR_PAIR (6),A_REVERSE))
#define REVERSE_COLOR         (PORT_COLOR (MY_COLOR_PAIR (7),A_REVERSE))
#define Q_SELECTED_COLOR      (PORT_COLOR (SELECTED_COLOR, 0))
#define Q_UNSELECTED_COLOR    REVERSE_COLOR

extern int sel_mark_color  [4];
extern int dialog_colors   [4];

/* Dialog colors */			   
#define COLOR_NORMAL       (PORT_COLOR (MY_COLOR_PAIR (8),A_REVERSE))
#define COLOR_FOCUS        (PORT_COLOR (MY_COLOR_PAIR (9),A_BOLD))
#define COLOR_HOT_NORMAL   (PORT_COLOR (MY_COLOR_PAIR (10),0))
#define COLOR_HOT_FOCUS    (PORT_COLOR (MY_COLOR_PAIR (11),0))
			   
#define VIEW_UNDERLINED_COLOR (PORT_COLOR (MY_COLOR_PAIR(12),A_UNDERLINE))
#define MENU_SELECTED_COLOR   (PORT_COLOR (MY_COLOR_PAIR(13),A_BOLD))
#define MENU_HOT_COLOR        (PORT_COLOR (MY_COLOR_PAIR(14),0))
#define MENU_HOTSEL_COLOR     (PORT_COLOR (MY_COLOR_PAIR(15),0))

#define HELP_NORMAL_COLOR  (PORT_COLOR (MY_COLOR_PAIR(16),A_REVERSE))
#define HELP_ITALIC_COLOR  (PORT_COLOR (MY_COLOR_PAIR(17),A_REVERSE))
#define HELP_BOLD_COLOR    (PORT_COLOR (MY_COLOR_PAIR(18),A_REVERSE))
#define HELP_LINK_COLOR    (PORT_COLOR (MY_COLOR_PAIR(19),0))
#define HELP_SLINK_COLOR   (PORT_COLOR (MY_COLOR_PAIR(20),A_BOLD))

/*
 * This should be selectable independently. Default has to be black background
 * foreground does not matter at all.
 */
#define GAUGE_COLOR        (PORT_COLOR (MY_COLOR_PAIR(21),0))
#define INPUT_COLOR        (PORT_COLOR (MY_COLOR_PAIR(22),0))

/* Add this to color panel, on BW all pairs are normal */
#define DIRECTORY_COLOR    (PORT_COLOR (MY_COLOR_PAIR (23),0))
#define EXECUTABLE_COLOR   (PORT_COLOR (MY_COLOR_PAIR (24),0))
#define LINK_COLOR         (PORT_COLOR (MY_COLOR_PAIR (25),0))
#define STALLED_LINK_COLOR (PORT_COLOR (MY_COLOR_PAIR (26),A_UNDERLINE))
#define DEVICE_COLOR       (PORT_COLOR (MY_COLOR_PAIR (27),0))
#define SPECIAL_COLOR      (PORT_COLOR (MY_COLOR_PAIR (28),0))
#define CORE_COLOR         (PORT_COLOR (MY_COLOR_PAIR (29),0))


#ifdef HAVE_SLANG
/* For the default color any unused index may be chosen. */
#    define DEFAULT_COLOR_INDEX   30
#    define DEFAULT_COLOR  (PORT_COLOR (MY_COLOR_PAIR(DEFAULT_COLOR_INDEX),0))
#   else
#     define DEFAULT_COLOR A_NORMAL
#endif

/*
 * editor colors - only 3 for normal, search->found, and select, respectively
 * Last is defined to view color.
 */
#define EDITOR_NORMAL_COLOR_INDEX     34
#define EDITOR_NORMAL_COLOR          (PORT_COLOR (MY_COLOR_PAIR (EDITOR_NORMAL_COLOR_INDEX), 0))
#define EDITOR_BOLD_COLOR            (PORT_COLOR (MY_COLOR_PAIR (35),A_BOLD))
#define EDITOR_MARKED_COLOR          (PORT_COLOR (MY_COLOR_PAIR (36),A_REVERSE))
#define EDITOR_UNDERLINED_COLOR      VIEW_UNDERLINED_COLOR

#endif /* __COLOR_H */

