
/** \file user.h
 *  \brief Header: user menu implementation
 */

#ifndef MC_USER_H
#define MC_USER_H

#include "lib/global.h"

struct WEdit;

void user_menu_cmd (struct WEdit *edit_widget);
char *expand_format (struct WEdit *edit_widget, char c, gboolean do_quote);
int check_format_view (const char *);
int check_format_var (const char *, char **);
int check_format_cd (const char *);

#endif					/* MC_USER_H */
