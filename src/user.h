
/** \file user.h
 *  \brief Header: user menu implementation
 */

#ifndef MC_USER_H
#define MC_USER_H

#include "../src/editor/edit.h"

void user_menu_cmd (WEdit *edit_widget);
char *expand_format (WEdit *edit_widget, char c, int quote);
int check_format_view (const char *);
int check_format_var (const char *, char **);
int check_format_cd (const char *);

#endif
