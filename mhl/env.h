#ifndef MHL_ENV_H
#define MHL_ENV_H

#include <mhl/string.h>

#define mhl_getenv_dup(name)	(mhl_str_dup(name ? getenv(name) : ""))

#endif
