#include <config.h>
#include "../src/fs.h"

#include <glib.h>

#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/main.h"

#include "../src/mem.h"
#include "../src/util.h"
#include "../src/mad.h"


#define BUF_4K		4096
#define BUF_1K		1024

#define BUF_LARGE	BUF_1K
#define BUF_MEDIUM	512
#define BUF_SMALL	128
#define BUF_TINY	64

char* append_path_sep (char *path);
