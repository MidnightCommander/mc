#ifndef __FILEGUI_H
#define __FILEGUI_H

/*
 * GUI callback routines
 */

void fmd_init_i18n (int force);

#ifdef WANT_WIDGETS
char *panel_get_file (WPanel *panel, struct stat *stat_buf);
#endif

#endif
