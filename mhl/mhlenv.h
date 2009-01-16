#ifndef __MHL_ENV_H
#define __MHL_ENV_H

#define mhl_getenv_dup(name)	(mhl_str_dup(name ? getenv(name) : ""))

#endif
