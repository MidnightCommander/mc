#ifndef __X_H
#define __X_H

#ifndef HAVE_X
#   include "textconf.h"
#endif

#ifdef HAVE_GNOME
#   define GNOME_REGEX_H
#   include <gnome.h>
#   include "gconf.h"
#   undef MIN
#   undef MAX
#   include "gmain.h"
#endif

#endif /* __X_H */
