#ifndef MC_COMPLETE_H
#define MC_COMPLETE_H

#define INPUT_COMPLETE_FILENAMES	 1
#define INPUT_COMPLETE_HOSTNAMES	 2
#define INPUT_COMPLETE_COMMANDS		 4
#define INPUT_COMPLETE_VARIABLES	 8
#define INPUT_COMPLETE_USERNAMES	16
#define INPUT_COMPLETE_CD		32

#include "widget.h"

void free_completions (WInput *);
void complete (WInput *);

#endif
