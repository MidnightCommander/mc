/* Defines which features are used on the GNOME edition */

#define PORT_HAS_FRONTEND_RUN_DLG        1
#define PORT_HAS_FILE_HANDLERS           1
#define PORT_HAS_GETCH                   1
#define PORT_HAS_MY_SYSTEM               1
#define PORT_HAS_DIALOG_TITLE            1
#define PORT_WANTS_CLEAN_BUTTON          1
#define PORT_HAS_CREATE_PANELS           1
#define PORT_HAS_PANEL_UPDATE_COLS       1
#define PORT_HAS_PAINT_FRAME             1
#define PORT_WANTS_GET_SORT_FN           1
#define PORT_HAS_LLINES                  1
#define PORT_HAS_LOAD_HINT               1
#define PORT_HAS_FILTER_CHANGED          1
#define PORT_HAS_PANEL_ADJUST_TOP_FILE   1
#define PORT_HAS_PANEL_RESET_SORT_LABELS 1
#define PORT_HAS_FLUSH_EVENTS            1
#define PORT_HAS_SET_IDLE                1
#define PORT_HAS_BACKTAB_CHAR            1
#define PORT_NOT_FOCUS_SELECT_ITEM       1
#define PORT_NOT_UNFOCUS_UNSELECT_ITEM   1
#define PORT_WANTS_VIEW                  1
#define PORT_HAS_VIEW_FREEZE             1
#define PORT_HAS_SAVE_PANEL_TYPES        1
#define PORT_HAS_DESTROY_CMD             1
#define PORT_HAS_RADIO_FOCUS_ITEM        1
#define PORT_HAS_DIALOG_STOP             1
#define PORT_HAS_DISPLAY_MINI_INFO       1

#define mi_getch() fprintf (stderr, "mi_getch is not implemented in this port\n")
#define frontend_run_dlg(x)        gtkrundlg_event (x)
