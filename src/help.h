#ifndef __HELP_H
#define __HELP_H

/* This file is included by help.c and man2hlp.c */

#define HELP_TEXT_WIDTH 58

/* Markers used in the help files */
#define CHAR_LINK_START		'\01'	/* Ctrl-A */
#define CHAR_LINK_POINTER	'\02'	/* Ctrl-B */
#define CHAR_LINK_END		'\03'	/* Ctrl-C */
#define CHAR_NODE_END		'\04'	/* Ctrl-D */
#define CHAR_ALTERNATE		'\05'	/* Ctrl-E */
#define CHAR_NORMAL		'\06'	/* Ctrl-F */
#define CHAR_VERSION		'\07'	/* Ctrl-G */
#define CHAR_FONT_BOLD		'\010'	/* Ctrl-H */
#define CHAR_FONT_NORMAL	'\013'	/* Ctrl-K */
#define CHAR_FONT_ITALIC	'\024'	/* Ctrl-T */

void interactive_display (const char *filename, const char *node);
#endif				/* __HELP_H */
