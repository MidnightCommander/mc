#ifndef __USER_H
#define __USER_H

void user_menu_cmd (void);
char *expand_format (char, int);
int check_format_view (const char *);
int check_format_var (const char *, char **);
int check_format_cd (const char *);
char *check_patterns (char*);

#ifdef OS2_NT
#   define MC_LOCAL_MENU  "mc.mnu"
#   define MC_GLOBAL_MENU "mc.mnu"
#   define MC_HOME_MENU   "mc.mnu"
#   define MC_HINT        "mc.hnt"
#else
#   define MC_GLOBAL_MENU "mc.menu"
#   define MC_LOCAL_MENU  ".mc.menu"
#   define MC_HOME_MENU   ".mc/menu"
#   define MC_HINT        "mc.hint"
#endif

#endif
