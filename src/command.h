#ifndef __COMMAND_H
#define __COMMAND_H

extern WInput *cmdline;

WInput *command_new (int y, int x, int len);
void do_cd_command (char *cmd);
void command_insert (WInput * in, const char *text, int insert_extra_space);

#endif /* __COMMAND_H */
