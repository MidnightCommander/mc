/* Defines which features are used on the Tk edition */

/* key.h defines */

#define PORT_HAS_FILE_HANDLERS     1
#define PORT_HAS_GETCH             1
#define PORT_HAS_FRONTEND_RUN_DLG  1
#define PORT_WANTS_GET_SORT_FN     1
#define frontend_run_dlg(x)        tkrundlg_event (x)

/* Other */
#define COMPUTE_FORMAT_ALLOCATIONS
