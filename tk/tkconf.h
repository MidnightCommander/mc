/* Defines which features are used on the Tk edition */

/* key.h defines */

#define PORT_HAS_FILE_HANDLERS      	 1
#define PORT_HAS_GETCH              	 1
#define PORT_HAS_FRONTEND_RUN_DLG   	 1
#define PORT_WANTS_GET_SORT_FN      	 1
#define PORT_HAS_FILTER_CHANGED     	 1
#define PORT_HAS_PANEL_ADJUST_TOP_FILE   1
#define PORT_HAS_PANEL_RESET_SORT_LABELS 1
#define PORT_HAS_DESTROY_CMD             1
#define PORT_HAS_RADIO_FOCUS_ITEM        1
#define PORT_HAS_DISPLAY_MINI_INFO       1

#define frontend_run_dlg(x)        tkrundlg_event (x)
#define port_shutdown_extra_fds()  
/* Other */
#define COMPUTE_FORMAT_ALLOCATIONS
