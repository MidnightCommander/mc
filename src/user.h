
/** \file user.h
 *  \brief Header: user menu implementation
 */

#ifndef MC_USER_H
#define MC_USER_H

#include "../edit/edit-widget.h"

void user_menu_cmd (struct WEdit *edit_widget);
char *expand_format (struct WEdit *edit_widget, char c, int quote);
int check_format_view (const char *);
int check_format_var (const char *, char **);
int check_format_cd (const char *);

#define MC_GLOBAL_MENU       "mc.menu"
#define MC_LOCAL_MENU        ".mc.menu"
#define MC_HOME_MENU         ".mc/menu"
#define MC_HINT              "mc.hint"

#endif
