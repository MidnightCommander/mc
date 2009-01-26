#include "recode.h"
#ifdef HAVE_CHARSET

char *lang;
char lang_codepage_name[256];
int lang_codepage;

int ftp_codepage=-1;

// recode buffer for displaying file names
unsigned char recode_buf[MC_MAXPATHLEN];

WPanel* recode_panel;

//--- get codepage from $LANG
void get_locale_codepage() {
  char* a;
  char* b;
  int len;

  lang=getenv("LANG");
  if(!lang) {
    strncpy(lang_codepage_name,OTHER_8BIT, sizeof(OTHER_8BIT));
    lang_codepage=-1;
    return;
  }

  a=strchr(lang,'.');
  if(!a) {
    strncpy(lang_codepage_name,OTHER_8BIT, sizeof(OTHER_8BIT));
    lang_codepage=-1;
    return;
  }
  ++a;

  b=strchr(lang,'@');
  if(!b) b=lang+strlen(lang);

  len=b-a;
  if(len>=sizeof(lang_codepage_name)) len=sizeof(lang_codepage_name)-1;

  memcpy(lang_codepage_name,a, len);
  lang_codepage_name[len]='\0';
  lang_codepage=get_codepage_index(lang_codepage_name);
  if(lang_codepage<0) strncpy(lang_codepage_name,OTHER_8BIT, sizeof(OTHER_8BIT));
}

//--- reset translation table
void  my_reset_tt(unsigned char *table,int n) {
  int i;
  for(i=0;i<n;i++) table[i]=i;
}

//--- reset panel codepage
void panel_reset_codepage(WPanel *p) {
  p->src_codepage=-1;
  my_reset_tt(p->tr_table,256);
  my_reset_tt(p->tr_table_input,256);
}

//--- Initialize translation table
//    i need this function because init_translation_table from
//    charsets.c fills only fixed translation tables conv_displ and conv_input
//---
char* my_init_tt( int from, int to, unsigned char *table) {
 int i;
 iconv_t cd;
 char *cpfrom, *cpto;

 if(from < 0 || to < 0 || from == to) {
   my_reset_tt(table,256);
   return NULL;
 }
 my_reset_tt(table,128);
 cpfrom=codepages[from ].id;
 cpto=codepages[to].id;
 cd=iconv_open(cpfrom, cpto);
 if(cd==(iconv_t)-1) {
   snprintf(errbuf, 255, _("Cannot translate from %s to %s"), cpfrom, cpto);
   return errbuf;
 }
 for(i=128; i<=255; ++i) table[i] = translate_character(cd, i);
 iconv_close(cd);
 return NULL;
}

//--- Translate string from one codepage to another
void my_translate_string(unsigned char *s1,int l1, unsigned char *s2, unsigned char *table) {
  int i=0;
  if(!s1) return;
  while(i<l1) {
    s2[i]=table[s1[i]];
    i++;
   }
  s2[i]=0;
}

//--- Recode filename and concat in to dir
char* concat_dir_and_recoded_fname(const char *dir, const char *fname, FileOpContext *ctx) {
    int i = strlen (dir);

    my_translate_string((unsigned char*)fname,strlen(fname),ctx->recode_buf,ctx->tr_table);
    if (dir [i-1] == PATH_SEP)
        return  g_strconcat (dir, ctx->recode_buf, NULL);
    else
        return  g_strconcat (dir, PATH_SEP_STR, ctx->recode_buf, NULL);
  return 0;
}


//--- Internal handler for "Panel codepage"
static void fnc_cmd(WPanel *p) {
  char *errmsg;
  if(display_codepage > 0) {
    p->src_codepage=select_charset(p->src_codepage, 0, _(" Choose panel codepage "));
    errmsg=my_init_tt(display_codepage,p->src_codepage,p->tr_table);
    if(errmsg) {
      panel_reset_codepage(p);
      message( 1, MSG_ERROR, "%s", errmsg);
    }
    errmsg=my_init_tt(p->src_codepage,display_codepage,p->tr_table_input);
    if (errmsg) {
      panel_reset_codepage(p);
      message( 1, MSG_ERROR, "%s", errmsg );
     }
    paint_dir(p);
    show_dir(p);
    display_mini_info(p);
  }
  else {
    message( 1, _(" Warning "),
                _("To use this feature select your codepage in\n"
                  "Setup / Display Bits dialog!\n"
                  "Do not forget to save options." ));
  }
}

//--- Menu handlers for "Panel codepage" for left and right panel menu

void fnc_l_cmd() {
  fnc_cmd(left_panel);
}

void fnc_r_cmd() {
  fnc_cmd(right_panel);
}

//--- screen handler for "Panel codepage"
void fnc_c_cmd(WPanel *panel) {
  fnc_cmd(current_panel);
}

#endif //HAVE_CHARSET
