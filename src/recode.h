#ifndef __RECODE_H__
#define __RECODE_H__
#include <config.h>
#ifdef HAVE_CHARSET

#include <stdio.h>
#include <locale.h>
#include <iconv.h>

#include "global.h"
#include "wtools.h"
#include "panel.h"
#include "charsets.h"
#include "selcodepage.h"
#include "screen.h"
#include "main.h"
#include "fileopctx.h"

extern char *lang;
extern char lang_codepage_name[256];
extern int lang_codepage;

extern int ftp_codepage;

// recode buffer for displaying file names
extern unsigned char recode_buf[MC_MAXPATHLEN];
extern WPanel* recode_panel;

//--- get codepage from $LANG
extern void get_locale_codepage();

//--- reset translation table
extern void  my_reset_tt(unsigned char *table,int n);
//--- reset panel codepage
extern void panel_reset_codepage(WPanel *p);
//--- Initialize translation table
extern char* my_init_tt( int from, int to, unsigned char *table);
//--- Translate string from one codepage to another
extern void my_translate_string(unsigned char *s1,int l1, unsigned char *s2, unsigned char *table);
//--- Recode filename and concat in to dir
extern char* concat_dir_and_recoded_fname(const char *dir, const char *fname, FileOpContext *ctx);
//--- handlers for "Panel codepage"
extern void fnc_l_cmd();
extern void fnc_r_cmd();
extern void fnc_c_cmd(WPanel *panel);

#endif // HAVE_CHARSET
#endif //__RECODE_H__
