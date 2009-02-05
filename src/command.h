
/** \file command.h
 *  \brief Header: command line widget
 */

#ifndef MC_COMMAND_H
#define MC_COMMAND_H

#include "widget.h"

extern WInput *cmdline;

WInput *command_new (int y, int x, int len);
void do_cd_command (char *cmd);
void command_insert (WInput * in, const char *text, int insert_extra_space);

#endif
