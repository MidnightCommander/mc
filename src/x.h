#ifndef __X_H
#define __X_H

#ifndef HAVE_X
#   include "textconf.h"
#endif

#ifdef HAVE_GNOME
#   ifdef HAVE_SYS_PARAM_H
#      include <sys/param.h>
#   endif
#   define GNOME_REGEX_H
#   include <gnome.h>
#   include "gconf.h"
#   include "gmain.h"
#endif

#endif /* __X_H */
