/* Defines which features are used on the XView edition */

#define PORT_HAS_GETCH                   1
#define PORT_HAS_FRONTEND_RUN_DLG        1
#define PORT_WIDGET_WANTS_HISTORY        1
#define mi_getch() fprintf (stderr, "mi_getch is not implemented\n")
#define frontend_run_dlg(x)        xvrundlg_event (x)
#define port_shutdown_extra_fds()
#define COMPUTE_FORMAT_ALLOCATIONS
