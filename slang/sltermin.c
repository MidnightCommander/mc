/* This file contains enough terminfo reading capabilities sufficient for
 * the slang SLtt interface.
 */

/*
Copyright (C) 2004, 2005 John E. Davis

This file is part of the S-Lang Library.

The S-Lang Library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The S-Lang Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.  
*/

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

/*
 * The majority of the comments found in the file were taken from the
 * term(4) man page on an SGI.
 */

/* Short integers are stored in two 8-bit bytes.  The first byte contains
 * the least significant 8 bits of the value, and the second byte contains
 * the most significant 8 bits.  (Thus, the value represented is
 * 256*second+first.)  The value -1 is represented by 0377,0377, and the
 * value -2 is represented by 0376,0377; other negative values are illegal.
 * The -1 generally means that a capability is missing from this terminal.
 * The -2 means that the capability has been cancelled in the terminfo
 * source and also is to be considered missing.
 */

static int make_integer (unsigned char *buf)
{
   register int lo, hi;
   lo = (int) *buf++; hi = (int) *buf;
   if (hi == 0377)
     {
	if (lo == 0377) return -1;
	if (lo == 0376) return -2;
     }
   return lo + 256 * hi;
}

/*
 * The compiled file is created from the source file descriptions of the
 * terminals (see the -I option of infocmp) by using the terminfo compiler,
 * tic, and read by the routine setupterm [see curses(3X).]  The file is
 * divided into six parts in the following order:  the header, terminal
 * names, boolean flags, numbers, strings, and string table.
 *
 * The header section begins the file.  This section contains six short
 * integers in the format described below.  These integers are (1) the magic
 * number (octal 0432); (2) the size, in bytes, of the names section; (3)
 * the number of bytes in the boolean section; (4) the number of short
 * integers in the numbers section; (5) the number of offsets (short
 * integers) in the strings section; (6) the size, in bytes, of the string
 * table.
 */

#define MAGIC 0432

/* In this structure, all char * fields are malloced EXCEPT if the
 * structure is SLTERMCAP.  In that case, only terminal_names is malloced
 * and the other fields are pointers into it.
 */
struct _pSLterminfo_Type
{
#define SLTERMINFO 1
#define SLTERMCAP  2
   unsigned int flags;

   unsigned int name_section_size;
   char *terminal_names;

   unsigned int boolean_section_size;
   unsigned char *boolean_flags;

   unsigned int num_numbers;
   unsigned char *numbers;

   unsigned int num_string_offsets;
   unsigned char *string_offsets;

   unsigned int string_table_size;
   char *string_table;

};

static char *tcap_getstr (char *, SLterminfo_Type *);
static int tcap_getnum (char *, SLterminfo_Type *);
static int tcap_getflag (char *, SLterminfo_Type *);
static int tcap_getent (char *, SLterminfo_Type *);

static FILE *open_terminfo (char *file, SLterminfo_Type *h)
{
   FILE *fp;
   unsigned char buf[12];

   /* Alan Cox reported a security problem here if the application using the
    * library is setuid.  So, I need to make sure open the file as a normal
    * user.  Unfortunately, there does not appear to be a portable way of
    * doing this, so I am going to use 'setfsgid' and 'setfsuid', which
    * are not portable.
    *
    * I will also look into the use of setreuid, seteuid and setregid, setegid.
    * FIXME: Priority=medium
    */
   fp = fopen (file, "rb");
   if (fp == NULL) return NULL;

   if ((12 == fread ((char *) buf, 1, 12, fp) && (MAGIC == make_integer (buf))))
     {
	h->name_section_size = make_integer (buf + 2);
	h->boolean_section_size = make_integer (buf + 4);
	h->num_numbers = make_integer (buf + 6);
	h->num_string_offsets = make_integer (buf + 8);
	h->string_table_size = make_integer (buf + 10);
     }
   else
     {
	fclose (fp);
	fp = NULL;
     }
   return fp;
}

/*
 * The terminal names section comes next.  It contains the first line of the
 * terminfo description, listing the various names for the terminal,
 * separated by the bar ( | ) character (see term(5)).  The section is
 * terminated with an ASCII NUL character.
 */

/* returns pointer to malloced space */
static unsigned char *read_terminfo_section (FILE *fp, unsigned int size)
{
   char *s;

   if (NULL == (s = (char *) SLmalloc (size))) return NULL;
   if (size != fread (s, 1, size, fp))
     {
	SLfree (s);
	return NULL;
     }
   return (unsigned char *) s;
}

static char *read_terminal_names (FILE *fp, SLterminfo_Type *t)
{
   return t->terminal_names = (char *) read_terminfo_section (fp, t->name_section_size);
}

/*
 * The boolean flags have one byte for each flag.  This byte is either 0 or
 * 1 as the flag is present or absent.  The value of 2 means that the flag
 * has been cancelled.  The capabilities are in the same order as the file
 * <term.h>.
 */

static unsigned char *read_boolean_flags (FILE *fp, SLterminfo_Type *t)
{
   /* Between the boolean section and the number section, a null byte is
    * inserted, if necessary, to ensure that the number section begins on an
    * even byte offset. All short integers are aligned on a short word
    * boundary.
    */

   unsigned int size = (t->name_section_size + t->boolean_section_size) % 2;
   size += t->boolean_section_size;

   return t->boolean_flags = read_terminfo_section (fp, size);
}

/*
 * The numbers section is similar to the boolean flags section.  Each
 * capability takes up two bytes, and is stored as a short integer.  If the
 * value represented is -1 or -2, the capability is taken to be missing.
 */

static unsigned char *read_numbers (FILE *fp, SLterminfo_Type *t)
{
   return t->numbers = read_terminfo_section (fp, 2 * t->num_numbers);
}

/* The strings section is also similar.  Each capability is stored as a
 * short integer, in the format above.  A value of -1 or -2 means the
 * capability is missing.  Otherwise, the value is taken as an offset from
 * the beginning of the string table.  Special characters in ^X or \c
 * notation are stored in their interpreted form, not the printing
 * representation.  Padding information ($<nn>) and parameter information
 * (%x) are stored intact in uninterpreted form.
 */

static unsigned char *read_string_offsets (FILE *fp, SLterminfo_Type *t)
{
   return t->string_offsets = (unsigned char *) read_terminfo_section (fp, 2 * t->num_string_offsets);
}

/* The final section is the string table.  It contains all the values of
 * string capabilities referenced in the string section.  Each string is
 * null terminated.
 */

static char *read_string_table (FILE *fp, SLterminfo_Type *t)
{
   return t->string_table = (char *) read_terminfo_section (fp, t->string_table_size);
}

/*
 * Compiled terminfo(4) descriptions are placed under the directory
 * /usr/share/lib/terminfo.  In order to avoid a linear search of a huge
 * UNIX system directory, a two-level scheme is used:
 * /usr/share/lib/terminfo/c/name where name is the name of the terminal,
 * and c is the first character of name.  Thus, att4425 can be found in the
 * file /usr/share/lib/terminfo/a/att4425.  Synonyms for the same terminal
 * are implemented by multiple links to the same compiled file.
 */

static char *Terminfo_Dirs [] =
{
   NULL, /* $HOME/.terminfo */
   NULL, /* $TERMINFO */
   "/usr/share/terminfo",
   "/usr/lib/terminfo",
   "/usr/share/lib/terminfo",
   "/etc/terminfo",
   "/usr/local/lib/terminfo",
#ifdef MISC_TERMINFO_DIRS
   MISC_TERMINFO_DIRS,
#endif
   ""
};

SLterminfo_Type *_pSLtt_tigetent (char *term)
{
   char *tidir;
   int i;
   FILE *fp = NULL;
   char file[1024];
   static char home_ti [1024];
   char *home;
   SLterminfo_Type *ti;

   if (
       (term == NULL)
#ifdef SLANG_UNTIC
       && (SLang_Untic_Terminfo_File == NULL)
#endif
       )
     return NULL;

   if (_pSLsecure_issetugid ()
       && ((term[0] == '.') || (NULL != strchr (term, '/'))))
     return NULL;

   if (NULL == (ti = (SLterminfo_Type *) SLmalloc (sizeof (SLterminfo_Type))))
     {
	return NULL;
     }

#ifdef SLANG_UNTIC
   if (SLang_Untic_Terminfo_File != NULL)
     {
	fp = open_terminfo (SLang_Untic_Terminfo_File, ti);
	goto fp_open_label;
     }
   else
#endif
   /* If we are on a termcap based system, use termcap */
   if (0 == tcap_getent (term, ti)) return ti;

   if (NULL != (home = _pSLsecure_getenv ("HOME")))
     {
	size_t len = strlen (home);
    
	if (len > sizeof (home_ti) - sizeof ("/.terminfo"))
	    len = sizeof (home_ti) - sizeof ("/.terminfo");
	memcpy (home_ti, home, len);
	memcpy (home_ti + len, "/.terminfo", sizeof ("/.terminfo"));
	Terminfo_Dirs [0] = home_ti;
     }

   Terminfo_Dirs[1] = _pSLsecure_getenv ("TERMINFO");
   i = 0;
   while (1)
     {
	tidir = Terminfo_Dirs[i];
	if (tidir != NULL)
	  {
	     if (*tidir == 0)
	       break;		       /* last one */

	     if (sizeof (file) >= strlen (tidir) + 4 + strlen (term))
	       {
		  sprintf (file, "%s/%c/%s", tidir, *term, term);
		  if (NULL != (fp = open_terminfo (file, ti)))
		    break;
	       }
	  }
	i++;
     }

#ifdef SLANG_UNTIC
   fp_open_label:
#endif

   if (fp != NULL)
     {
	if (NULL != read_terminal_names (fp, ti))
	  {
	     if (NULL != read_boolean_flags (fp, ti))
	       {
		  if (NULL != read_numbers (fp, ti))
		    {
		       if (NULL != read_string_offsets (fp, ti))
			 {
			    if (NULL != read_string_table (fp, ti))
			      {
				 /* success */
				 fclose (fp);
				 ti->flags = SLTERMINFO;
				 return ti;
			      }
			    SLfree ((char *)ti->string_offsets);
			 }
		       SLfree ((char *)ti->numbers);
		    }
		  SLfree ((char *)ti->boolean_flags);
	       }
	     SLfree ((char *)ti->terminal_names);
	  }
	fclose (fp);
     }

   SLfree ((char *)ti);
   return NULL;
}

#ifdef SLANG_UNTIC
# define UNTIC_COMMENT(x) ,x
#else
# define UNTIC_COMMENT(x)
#endif

typedef SLCONST struct
{
   char name[3];
   int offset;
#ifdef SLANG_UNTIC
   char *comment;
#endif
}
Tgetstr_Map_Type;

/* I need to add: K1-5, %0-5(not important), @8, &8... */
static Tgetstr_Map_Type Tgetstr_Map [] =
{
   {"!1", 212		UNTIC_COMMENT("shifted key")},
   {"!2", 213		UNTIC_COMMENT("shifted key")},
   {"!3", 214		UNTIC_COMMENT("shifted key")},
   {"#1", 198		UNTIC_COMMENT("shifted key")},
   {"#2", 199		UNTIC_COMMENT("Key S-Home")},
   {"#3", 200		UNTIC_COMMENT("Key S-Insert")},
   {"#4", 201		UNTIC_COMMENT("Key S-Left")},
   {"%0", 177		UNTIC_COMMENT("redo key")},
   {"%1", 168		UNTIC_COMMENT("help key")},
   {"%2", 169		UNTIC_COMMENT("mark key")},
   {"%3", 170		UNTIC_COMMENT("message key")},
   {"%4", 171		UNTIC_COMMENT("move key")},
   {"%5", 172		UNTIC_COMMENT("next key")},
   {"%6", 173		UNTIC_COMMENT("open key")},
   {"%7", 174		UNTIC_COMMENT("options key")},
   {"%8", 175		UNTIC_COMMENT("previous key")},
   {"%9", 176		UNTIC_COMMENT("print key")},
   {"%a", 202		UNTIC_COMMENT("shifted key")},
   {"%b", 203		UNTIC_COMMENT("shifted key")},
   {"%c", 204		UNTIC_COMMENT("Key S-Next")},
   {"%d", 205		UNTIC_COMMENT("shifted key")},
   {"%e", 206		UNTIC_COMMENT("Key S-Previous")},
   {"%f", 207		UNTIC_COMMENT("shifted key")},
   {"%g", 208		UNTIC_COMMENT("shifted key")},
   {"%h", 209		UNTIC_COMMENT("shifted key")},
   {"%i", 210		UNTIC_COMMENT("Key S-Right")},
   {"%j", 211		UNTIC_COMMENT("shifted key")},
   {"&0", 187		UNTIC_COMMENT("shifted key")},
   {"&1", 178		UNTIC_COMMENT("reference key")},
   {"&2", 179		UNTIC_COMMENT("refresh key")},
   {"&3", 180		UNTIC_COMMENT("replace key")},
   {"&4", 181		UNTIC_COMMENT("restart key")},
   {"&5", 182		UNTIC_COMMENT("resume key")},
   {"&6", 183		UNTIC_COMMENT("save key")},
   {"&7", 184		UNTIC_COMMENT("suspend key")},
   {"&8", 185		UNTIC_COMMENT("undo key")},
   {"&9", 186		UNTIC_COMMENT("shifted key")},
   {"*0", 197		UNTIC_COMMENT("shifted key")},
   {"*1", 188		UNTIC_COMMENT("shifted key")},
   {"*2", 189		UNTIC_COMMENT("shifted key")},
   {"*3", 190		UNTIC_COMMENT("shifted key")},
   {"*4", 191		UNTIC_COMMENT("Key S-Delete")},
   {"*5", 192		UNTIC_COMMENT("shifted key")},
   {"*6", 193		UNTIC_COMMENT("select key")},
   {"*7", 194		UNTIC_COMMENT("Key S-End")},
   {"*8", 195		UNTIC_COMMENT("shifted key")},
   {"*9", 196		UNTIC_COMMENT("shifted key")},
   {"@0", 167		UNTIC_COMMENT("find key")},
   {"@1", 158		UNTIC_COMMENT("begin key")},
   {"@2", 159		UNTIC_COMMENT("cancel key")},
   {"@3", 160		UNTIC_COMMENT("close key")},
   {"@4", 161		UNTIC_COMMENT("command key")},
   {"@5", 162		UNTIC_COMMENT("copy key")},
   {"@6", 163		UNTIC_COMMENT("create key")},
   {"@7", 164 		UNTIC_COMMENT("Key End")},
   {"@8", 165		UNTIC_COMMENT("enter/send key")},
   {"@9", 166		UNTIC_COMMENT("exit key")},
   {"AB", 360 		UNTIC_COMMENT("set ANSI color background")},
   {"AF", 359 		UNTIC_COMMENT("set ANSI color foreground")},
   {"AL", 110 		UNTIC_COMMENT("parm_insert_line")},
   {"CC", 9		UNTIC_COMMENT("terminal settable cmd character in prototype !?")},
   {"CM", 15		UNTIC_COMMENT("memory relative cursor addressing")},
   {"CW", 277		UNTIC_COMMENT("define a window #1 from #2, #3 to #4, #5")},
   {"DC", 105		UNTIC_COMMENT("delete #1 chars")},
   {"DI", 280		UNTIC_COMMENT("dial number #1")},
   {"DK", 275		UNTIC_COMMENT("display clock at (#1,#2)")},
   {"DL", 106 		UNTIC_COMMENT("parm_delete_line")},
   {"DO", 107		UNTIC_COMMENT("down #1 lines")},
   {"F1", 216		UNTIC_COMMENT("key_f11")},
   {"F2", 217		UNTIC_COMMENT("key_f12")},
   {"F3", 218		UNTIC_COMMENT("key_f13")},
   {"F4", 219		UNTIC_COMMENT("key_f14")},
   {"F5", 220		UNTIC_COMMENT("key_f15")},
   {"F6", 221		UNTIC_COMMENT("key_f16")},
   {"F7", 222		UNTIC_COMMENT("key_f17")},
   {"F8", 223		UNTIC_COMMENT("key_f18")},
   {"F9", 224		UNTIC_COMMENT("key_f19")},
   {"FA", 225		UNTIC_COMMENT("key_f20")},
   {"FB", 226		UNTIC_COMMENT("F21 function key")},
   {"FC", 227		UNTIC_COMMENT("F22 function key")},
   {"FD", 228		UNTIC_COMMENT("F23 function key")},
   {"FE", 229		UNTIC_COMMENT("F24 function key")},
   {"FF", 230		UNTIC_COMMENT("F25 function key")},
   {"FG", 231		UNTIC_COMMENT("F26 function key")},
   {"FH", 232		UNTIC_COMMENT("F27 function key")},
   {"FI", 233		UNTIC_COMMENT("F28 function key")},
   {"FJ", 234		UNTIC_COMMENT("F29 function key")},
   {"FK", 235		UNTIC_COMMENT("F30 function key")},
   {"FL", 236		UNTIC_COMMENT("F31 function key")},
   {"FM", 237		UNTIC_COMMENT("F32 function key")},
   {"FN", 238		UNTIC_COMMENT("F33 function key")},
   {"FO", 239		UNTIC_COMMENT("F34 function key")},
   {"FP", 240		UNTIC_COMMENT("F35 function key")},
   {"FQ", 241		UNTIC_COMMENT("F36 function key")},
   {"FR", 242		UNTIC_COMMENT("F37 function key")},
   {"FS", 243		UNTIC_COMMENT("F38 function key")},
   {"FT", 244		UNTIC_COMMENT("F39 function key")},
   {"FU", 245		UNTIC_COMMENT("F40 function key")},
   {"FV", 246		UNTIC_COMMENT("F41 function key")},
   {"FW", 247		UNTIC_COMMENT("F42 function key")},
   {"FX", 248		UNTIC_COMMENT("F43 function key")},
   {"FY", 249		UNTIC_COMMENT("F44 function key")},
   {"FZ", 250		UNTIC_COMMENT("F45 function key")},
   {"Fa", 251		UNTIC_COMMENT("F46 function key")},
   {"Fb", 252		UNTIC_COMMENT("F47 function key")},
   {"Fc", 253		UNTIC_COMMENT("F48 function key")},
   {"Fd", 254		UNTIC_COMMENT("F49 function key")},
   {"Fe", 255		UNTIC_COMMENT("F50 function key")},
   {"Ff", 256		UNTIC_COMMENT("F51 function key")},
   {"Fg", 257		UNTIC_COMMENT("F52 function key")},
   {"Fh", 258		UNTIC_COMMENT("F53 function key")},
   {"Fi", 259		UNTIC_COMMENT("F54 function key")},
   {"Fj", 260		UNTIC_COMMENT("F55 function key")},
   {"Fk", 261		UNTIC_COMMENT("F56 function key")},
   {"Fl", 262		UNTIC_COMMENT("F57 function key")},
   {"Fm", 263		UNTIC_COMMENT("F58 function key")},
   {"Fn", 264		UNTIC_COMMENT("F59 function key")},
   {"Fo", 265		UNTIC_COMMENT("F60 function key")},
   {"Fp", 266		UNTIC_COMMENT("F61 function key")},
   {"Fq", 267		UNTIC_COMMENT("F62 function key")},
   {"Fr", 268		UNTIC_COMMENT("F63 function key")},
   {"G1", 400		UNTIC_COMMENT("single upper right")},
   {"G2", 398		UNTIC_COMMENT("single upper left")},
   {"G3", 399		UNTIC_COMMENT("single lower left")},
   {"G4", 401		UNTIC_COMMENT("single lower right")},
   {"GC", 408		UNTIC_COMMENT("single intersection")},
   {"GD", 405		UNTIC_COMMENT("tee pointing down")},
   {"GH", 406		UNTIC_COMMENT("single horizontal line")},
   {"GL", 403		UNTIC_COMMENT("tee pointing left")},
   {"GR", 402		UNTIC_COMMENT("tee pointing right")},
   {"GU", 404		UNTIC_COMMENT("tee pointing up")},
   {"GV", 407		UNTIC_COMMENT("single vertical line")},
   {"Gm", 358		UNTIC_COMMENT("Curses should get button events")},
   {"HU", 279		UNTIC_COMMENT("hang-up phone")},
   {"IC", 108		UNTIC_COMMENT("insert #1 chars")},
   {"Ic", 299		UNTIC_COMMENT("initialize color #1 to (#2,#3,#4)")},
   {"Ip", 300		UNTIC_COMMENT("Initialize color pair #1 to fg=(#2,#3,#4), bg=(#5,#6,#7)")},
   {"K1", 139		UNTIC_COMMENT("upper left of keypad")},
   {"K2", 141		UNTIC_COMMENT("center of keypad")},
   {"K3", 140		UNTIC_COMMENT("upper right of keypad")},
   {"K4", 142		UNTIC_COMMENT("lower left of keypad")},
   {"K5", 143		UNTIC_COMMENT("lower right of keypad")},
   {"Km", 355		UNTIC_COMMENT("Mouse event has occurred")},
   {"LE", 111		UNTIC_COMMENT("move #1 chars to the left")},
   {"LF", 157		UNTIC_COMMENT("turn off soft labels")},
   {"LO", 156		UNTIC_COMMENT("turn on soft labels")},
   {"Lf", 273		UNTIC_COMMENT("label format")},
   {"MC", 270		UNTIC_COMMENT("clear right and left soft margins")},
   {"ML", 271		UNTIC_COMMENT("set left soft margin")},
   {"ML", 368		UNTIC_COMMENT("Set both left and right margins to #1, #2")},
   {"MR", 272		UNTIC_COMMENT("set right soft margin")},
   {"MT", 369		UNTIC_COMMENT("Sets both top and bottom margins to #1, #2")},
   {"Mi", 356		UNTIC_COMMENT("Mouse status information")},
   {"PA", 285		UNTIC_COMMENT("pause for 2-3 seconds")},
   {"PU", 283		UNTIC_COMMENT("select pulse dialling")},
   {"QD", 281		UNTIC_COMMENT("dial number #1 without checking")},
   {"RA", 152		UNTIC_COMMENT("turn off automatic margins")},
   {"RC", 276		UNTIC_COMMENT("remove clock")},
   {"RF", 215		UNTIC_COMMENT("send next input char (for ptys)")},
   {"RI", 112 		UNTIC_COMMENT("parm_right_cursor")},
   {"RQ", 357		UNTIC_COMMENT("Request mouse position")},
   {"RX", 150		UNTIC_COMMENT("turn off xon/xoff handshaking")},
   {"S1", 378		UNTIC_COMMENT("Display PC character")},
   {"S2", 379		UNTIC_COMMENT("Enter PC character display mode")},
   {"S3", 380		UNTIC_COMMENT("Exit PC character display mode")},
   {"S4", 381		UNTIC_COMMENT("Enter PC scancode mode")},
   {"S5", 382		UNTIC_COMMENT("Exit PC scancode mode")},
   {"S6", 383		UNTIC_COMMENT("PC terminal options")},
   {"S7", 384		UNTIC_COMMENT("Escape for scancode emulation")},
   {"S8", 385		UNTIC_COMMENT("Alternate escape for scancode emulation")},
   {"SA", 151		UNTIC_COMMENT("turn on automatic margins")},
   {"SC", 274		UNTIC_COMMENT("set clock, #1 hrs #2 mins #3 secs")},
   {"SF", 109		UNTIC_COMMENT("scroll forward #1 lines")},
   {"SR", 113		UNTIC_COMMENT("scroll back #1 lines")},
   {"SX", 149		UNTIC_COMMENT("turn on xon/xoff handshaking")},
   {"Sb", 303 		UNTIC_COMMENT("set background (color)")},
   {"Sf", 302 		UNTIC_COMMENT("set foreground (color)")},
   {"TO", 282		UNTIC_COMMENT("select touch tone dialing")},
   {"UP", 114		UNTIC_COMMENT("up #1 lines")},
   {"WA", 286		UNTIC_COMMENT("wait for dial-tone")},
   {"WG", 278		UNTIC_COMMENT("go to window #1")},
   {"XF", 154		UNTIC_COMMENT("XOFF character")},
   {"XN", 153		UNTIC_COMMENT("XON character")},
   {"Xh", 386		UNTIC_COMMENT("Enter horizontal highlight mode")},
   {"Xl", 387		UNTIC_COMMENT("Enter left highlight mode")},
   {"Xo", 388		UNTIC_COMMENT("Enter low highlight mode")},
   {"Xr", 389		UNTIC_COMMENT("Enter right highlight mode")},
   {"Xt", 390		UNTIC_COMMENT("Enter top highlight mode")},
   {"Xv", 391		UNTIC_COMMENT("Enter vertical highlight mode")},
   {"Xy", 370		UNTIC_COMMENT("Repeat bit image cell #1 #2 times")},
   {"YZ", 377		UNTIC_COMMENT("Set page length to #1 lines")},
   {"Yv", 372		UNTIC_COMMENT("Move to beginning of same row")},
   {"Yw", 373		UNTIC_COMMENT("Give name for color #1")},
   {"Yx", 374		UNTIC_COMMENT("Define rectangualar bit image region")},
   {"Yy", 375		UNTIC_COMMENT("End a bit-image region")},
   {"Yz", 376		UNTIC_COMMENT("Change to ribbon color #1")},
   {"ZA", 304		UNTIC_COMMENT("Change number of characters per inch")},
   {"ZB", 305		UNTIC_COMMENT("Change number of lines per inch")},
   {"ZC", 306		UNTIC_COMMENT("Change horizontal resolution")},
   {"ZD", 307		UNTIC_COMMENT("Change vertical resolution")},
   {"ZE", 308		UNTIC_COMMENT("Define a character")},
   {"ZF", 309		UNTIC_COMMENT("Enter double-wide mode")},
   {"ZG", 310		UNTIC_COMMENT("Enter draft-quality mode")},
   {"ZH", 311		UNTIC_COMMENT("Enter italic mode")},
   {"ZI", 312		UNTIC_COMMENT("Start leftward carriage motion")},
   {"ZJ", 313		UNTIC_COMMENT("Start micro-motion mode")},
   {"ZK", 314		UNTIC_COMMENT("Enter NLQ mode")},
   {"ZL", 315		UNTIC_COMMENT("Wnter normal-quality mode")},
   {"ZM", 316		UNTIC_COMMENT("Enter shadow-print mode")},
   {"ZN", 317		UNTIC_COMMENT("Enter subscript mode")},
   {"ZO", 318		UNTIC_COMMENT("Enter superscript mode")},
   {"ZP", 319		UNTIC_COMMENT("Start upward carriage motion")},
   {"ZQ", 320		UNTIC_COMMENT("End double-wide mode")},
   {"ZR", 321		UNTIC_COMMENT("End italic mode")},
   {"ZS", 322		UNTIC_COMMENT("End left-motion mode")},
   {"ZT", 323		UNTIC_COMMENT("End micro-motion mode")},
   {"ZU", 324		UNTIC_COMMENT("End shadow-print mode")},
   {"ZV", 325		UNTIC_COMMENT("End subscript mode")},
   {"ZW", 326		UNTIC_COMMENT("End superscript mode")},
   {"ZX", 327		UNTIC_COMMENT("End reverse character motion")},
   {"ZY", 328		UNTIC_COMMENT("Like column_address in micro mode")},
   {"ZZ", 329		UNTIC_COMMENT("Like cursor_down in micro mode")},
   {"Za", 330		UNTIC_COMMENT("Like cursor_left in micro mode")},
   {"Zb", 331		UNTIC_COMMENT("Like cursor_right in micro mode")},
   {"Zc", 332		UNTIC_COMMENT("Like row_address in micro mode")},
   {"Zd", 333		UNTIC_COMMENT("Like cursor_up in micro mode")},
   {"Ze", 334		UNTIC_COMMENT("Match software bits to print-head pins")},
   {"Zf", 335		UNTIC_COMMENT("Like parm_down_cursor in micro mode")},
   {"Zg", 336		UNTIC_COMMENT("Like parm_left_cursor in micro mode")},
   {"Zh", 337		UNTIC_COMMENT("Like parm_right_cursor in micro mode")},
   {"Zi", 338		UNTIC_COMMENT("Like parm_up_cursor in micro mode")},
   {"Zj", 339		UNTIC_COMMENT("Select character set")},
   {"Zk", 340		UNTIC_COMMENT("Set bottom margin at current line")},
   {"Zl", 341		UNTIC_COMMENT("Set bottom margin at line #1 or #2 lines from bottom")},
   {"Zm", 342		UNTIC_COMMENT("Set left (right) margin at column #1 (#2)")},
   {"Zn", 343		UNTIC_COMMENT("Set right margin at column #1")},
   {"Zo", 344		UNTIC_COMMENT("Set top margin at current line")},
   {"Zp", 345		UNTIC_COMMENT("Set top (bottom) margin at row #1 (#2)")},
   {"Zq", 346		UNTIC_COMMENT("Start printing bit image braphics")},
   {"Zr", 347		UNTIC_COMMENT("Start character set definition")},
   {"Zs", 348		UNTIC_COMMENT("Stop printing bit image graphics")},
   {"Zt", 349		UNTIC_COMMENT("End definition of character aet")},
   {"Zu", 350		UNTIC_COMMENT("List of subscriptable characters")},
   {"Zv", 351		UNTIC_COMMENT("List of superscriptable characters")},
   {"Zw", 352		UNTIC_COMMENT("Printing any of these chars causes CR")},
   {"Zx", 353		UNTIC_COMMENT("No motion for subsequent character")},
   {"Zy", 354		UNTIC_COMMENT("List of character set names")},
   {"Zz", 371		UNTIC_COMMENT("Move to next row of the bit image")},
   {"ac", 146 		UNTIC_COMMENT("acs_chars")},
   {"ae", 38 		UNTIC_COMMENT("exit_alt_charset_mode")},
   {"al", 53		UNTIC_COMMENT("insert line")},
   {"as", 25 		UNTIC_COMMENT("enter_alt_charset_mode")},
   {"bc", 395		UNTIC_COMMENT("move left, if not ^H")},
   {"bl", 1		UNTIC_COMMENT("audible signal (bell)")},
   {"bt", 0		UNTIC_COMMENT("back tab")},
   {"bx", 411		UNTIC_COMMENT("box chars primary set")},
   {"cb", 269		UNTIC_COMMENT("Clear to beginning of line")},
   {"cd", 7		UNTIC_COMMENT("clear to end of screen")},
   {"ce", 6 		UNTIC_COMMENT("clr_eol")},
   {"ch", 8		UNTIC_COMMENT("horizontal position #1, absolute")},
   {"ci", 363		UNTIC_COMMENT("Init sequence for multiple codesets")},
   {"cl", 5		UNTIC_COMMENT("clear screen and home cursor")},
   {"cm", 10		UNTIC_COMMENT("move to row #1 columns #2")},
   {"cr", 2		UNTIC_COMMENT("carriage return")},
   {"cs", 3		UNTIC_COMMENT("change region to line #1 to line #2")},
   {"ct", 4		UNTIC_COMMENT("clear all tab stops")},
   {"cv", 127		UNTIC_COMMENT("vertical position #1 absolute")},
   {"dc", 21		UNTIC_COMMENT("delete character")},
   {"dl", 22		UNTIC_COMMENT("delete line")},
   {"dm", 29		UNTIC_COMMENT("enter delete mode")},
   {"do", 11		UNTIC_COMMENT("down one line")},
   {"ds", 23		UNTIC_COMMENT("disable status line")},
   {"dv", 362		UNTIC_COMMENT("Indicate language/codeset support")},
   {"eA", 155		UNTIC_COMMENT("enable alternate char set")},
   {"ec", 37		UNTIC_COMMENT("erase #1 characters")},
   {"ed", 41		UNTIC_COMMENT("end delete mode")},
   {"ei", 42		UNTIC_COMMENT("exit insert mode")},
   {"ff", 46		UNTIC_COMMENT("hardcopy terminal page eject")},
   {"fh", 284		UNTIC_COMMENT("flash switch hook")},
   {"fs", 47		UNTIC_COMMENT("return from status line")},
   {"hd", 24		UNTIC_COMMENT("half a line down")},
   {"ho", 12		UNTIC_COMMENT("home cursor (if no cup)")},
   {"hu", 137		UNTIC_COMMENT("half a line up")},
   {"i1", 48		UNTIC_COMMENT("initialization string")},
   {"i2", 392		UNTIC_COMMENT("secondary initialization string")},
   {"i3", 50		UNTIC_COMMENT("initialization string")},
   {"iP", 138		UNTIC_COMMENT("path name of program for initialization")},
   {"ic", 52		UNTIC_COMMENT("insert character")},
   {"if", 51		UNTIC_COMMENT("name of initialization file")},
   {"im", 31		UNTIC_COMMENT("enter insert mode")},
   {"ip", 54		UNTIC_COMMENT("insert padding after inserted character")},
   {"is", 49		UNTIC_COMMENT("initialization string")},
   {"k0", 65		UNTIC_COMMENT("F0 function key")},
   {"k1", 66		UNTIC_COMMENT("F1 function key")},
   {"k2", 68		UNTIC_COMMENT("F2 function key")},
   {"k3", 69		UNTIC_COMMENT("F3 function key")},
   {"k4", 70		UNTIC_COMMENT("F4 function key")},
   {"k5", 71		UNTIC_COMMENT("F5 function key")},
   {"k6", 72		UNTIC_COMMENT("F6 function key")},
   {"k7", 73		UNTIC_COMMENT("F7 function key")},
   {"k8", 74		UNTIC_COMMENT("F8 fucntion key")},
   {"k9", 75		UNTIC_COMMENT("F9 function key")},
   {"k;", 67		UNTIC_COMMENT("F10 function key")},
   {"kA", 78		UNTIC_COMMENT("insert-line key")},
   {"kB", 148		UNTIC_COMMENT("back-tab key")},
   {"kC", 57		UNTIC_COMMENT("clear-screen or erase key")},
   {"kD", 59		UNTIC_COMMENT("delete-character key")},
   {"kE", 63		UNTIC_COMMENT("clear-to-end-of-line key")},
   {"kF", 84		UNTIC_COMMENT("scroll-forward key")},
   {"kH", 80		UNTIC_COMMENT("last-line key")},
   {"kI", 77		UNTIC_COMMENT("insert-character key")},
   {"kL", 60		UNTIC_COMMENT("delete-line key")},
   {"kM", 62		UNTIC_COMMENT("sent by rmir or smir in insert mode")},
   {"kN", 81		UNTIC_COMMENT("next-page key")},
   {"kP", 82		UNTIC_COMMENT("prev-page key")},
   {"kR", 85		UNTIC_COMMENT("scroll-backward key")},
   {"kS", 64		UNTIC_COMMENT("clear-to-end-of-screen key")},
   {"kT", 86		UNTIC_COMMENT("set-tab key")},
   {"ka", 56		UNTIC_COMMENT("clear-all-tabs key")},
   {"kb", 55		UNTIC_COMMENT("backspace key")},
   {"kd", 61		UNTIC_COMMENT("down-arrow key")},
   {"ke", 88		UNTIC_COMMENT("leave 'keyboard_transmit' mode")},
   {"kh", 76		UNTIC_COMMENT("home key")},
   {"kl", 79		UNTIC_COMMENT("left-arrow key")},
   {"ko", 396		UNTIC_COMMENT("list of self-mapped keycaps")},
   {"kr", 83		UNTIC_COMMENT("right-arrow key")},
   {"ks", 89		UNTIC_COMMENT("enter 'keyboard_transmit' mode")},
   {"kt", 58		UNTIC_COMMENT("clear-tab key")},
   {"ku", 87		UNTIC_COMMENT("up-arrow key")},
   {"l0", 90		UNTIC_COMMENT("label on function key f0 if not f0")},
   {"l1", 91		UNTIC_COMMENT("label on function key f1 if not f1")},
   {"l2", 93		UNTIC_COMMENT("label on function key f2 if not f2")},
   {"l3", 94		UNTIC_COMMENT("label on function key f3 if not f3")},
   {"l4", 95		UNTIC_COMMENT("label on function key f4 if not f4")},
   {"l5", 96		UNTIC_COMMENT("lable on function key f5 if not f5")},
   {"l6", 97		UNTIC_COMMENT("label on function key f6 if not f6")},
   {"l7", 98		UNTIC_COMMENT("label on function key f7 if not f7")},
   {"l8", 99		UNTIC_COMMENT("label on function key f8 if not f8")},
   {"l9", 100		UNTIC_COMMENT("label on function key f9 if not f9")},
   {"la", 92		UNTIC_COMMENT("label on function key f10 if not f10")},
   {"le", 14		UNTIC_COMMENT("move left one space")},
   {"ll", 18		UNTIC_COMMENT("last line, first column (if no cup)")},
   {"ma", 397		UNTIC_COMMENT("map arrow keys rogue(1) motion keys")},
   {"mb", 26		UNTIC_COMMENT("turn on blinking")},
   {"md", 27		UNTIC_COMMENT("turn on bold (extra bright) mode")},
   {"me", 39		UNTIC_COMMENT("turn off all attributes")},
   {"mh", 30		UNTIC_COMMENT("turn on half-bright mode")},
   {"mk", 32		UNTIC_COMMENT("turn on blank mode (characters invisible)")},
   {"ml", 409		UNTIC_COMMENT("memory lock above")},
   {"mm", 102		UNTIC_COMMENT("turn on meta mode (8th-bit on)")},
   {"mo", 101		UNTIC_COMMENT("turn off meta mode")},
   {"mp", 33		UNTIC_COMMENT("turn on protected mode")},
   {"mr", 34		UNTIC_COMMENT("turn on reverse video mode")},
   {"mu", 410		UNTIC_COMMENT("memory unlock")},
   {"nd", 17		UNTIC_COMMENT("move right one space")},
   {"nl", 394		UNTIC_COMMENT("use to move down")},
   {"nw", 103		UNTIC_COMMENT("newline (behave like cr followed by lf)")},
   {"oc", 298		UNTIC_COMMENT("Set all color pairs to the original ones")},
   {"op", 297		UNTIC_COMMENT("Set default pair to its original value")},
   {"pO", 144		UNTIC_COMMENT("turn on printer for #1 bytes")},
   {"pc", 104		UNTIC_COMMENT("padding char (instead of null)")},
   {"pf", 119		UNTIC_COMMENT("turn off printer")},
   {"pk", 115		UNTIC_COMMENT("program function key #1 to type string #2")},
   {"pl", 116		UNTIC_COMMENT("program function key #1 to execute string #2")},
   {"pn", 147		UNTIC_COMMENT("program label #1 to show string #2")},
   {"po", 120		UNTIC_COMMENT("turn on printer")},
   {"ps", 118		UNTIC_COMMENT("print contents of screen")},
   {"px", 117		UNTIC_COMMENT("program function key #1 to transmit string #2")},
   {"r1", 122		UNTIC_COMMENT("reset string")},
   {"r2", 123		UNTIC_COMMENT("reset string")},
   {"r3", 124		UNTIC_COMMENT("reset string")},
   {"rP", 145		UNTIC_COMMENT("like ip but when in insert mode")},
   {"rc", 126		UNTIC_COMMENT("restore cursor to last position of sc")},
   {"rf", 125		UNTIC_COMMENT("name of reset file")},
   {"rp", 121		UNTIC_COMMENT("repeat char #1 #2 times")},
   {"rs", 393		UNTIC_COMMENT("terminal reset string")},
   {"s0", 364		UNTIC_COMMENT("Shift to code set 0 (EUC set 0, ASCII)")},
   {"s1", 365		UNTIC_COMMENT("Shift to code set 1")},
   {"s2", 366		UNTIC_COMMENT("Shift to code set 2")},
   {"s3", 367		UNTIC_COMMENT("Shift to code set 3")},
   {"sa", 131		UNTIC_COMMENT("define video attributes #1-#9 (PG9)")},
   {"sc", 128		UNTIC_COMMENT("save current cursor position")},
   {"se", 43		UNTIC_COMMENT("exit standout mode")},
   {"sf", 129		UNTIC_COMMENT("scroll text up")},
   {"so", 35		UNTIC_COMMENT("begin standout mode")},
   {"sp", 301		UNTIC_COMMENT("Set current color pair to #1")},
   {"sr", 130		UNTIC_COMMENT("scroll text down")},
   {"st", 132		UNTIC_COMMENT("set a tab in every row, current columns")},
   {"ta", 134		UNTIC_COMMENT("tab to next 8-space hardware tab stop")},
   {"te", 40		UNTIC_COMMENT("strings to end programs using cup")},
   {"ti", 28		UNTIC_COMMENT("string to start programs using cup")},
   {"ts", 135		UNTIC_COMMENT("move to status line")},
   {"u0", 287		UNTIC_COMMENT("User string #0")},
   {"u1", 288		UNTIC_COMMENT("User string #1")},
   {"u2", 289		UNTIC_COMMENT("User string #2")},
   {"u3", 290		UNTIC_COMMENT("User string #3")},
   {"u4", 291		UNTIC_COMMENT("User string #4")},
   {"u5", 292		UNTIC_COMMENT("User string #5")},
   {"u6", 293		UNTIC_COMMENT("User string #6")},
   {"u7", 294		UNTIC_COMMENT("User string #7")},
   {"u8", 295		UNTIC_COMMENT("User string #8")},
   {"u9", 296		UNTIC_COMMENT("User string #9")},
   {"uc", 136		UNTIC_COMMENT("underline char and move past it")},
   {"ue", 44		UNTIC_COMMENT("exit underline mode")},
   {"up", 19		UNTIC_COMMENT("up one line")},
   {"us", 36		UNTIC_COMMENT("begin underline mode")},
   {"vb", 45		UNTIC_COMMENT("visible bell (may not move cursor)")},
   {"ve", 16		UNTIC_COMMENT("make cursor appear normal (undo civis/cvvis)")},
   {"vi", 13		UNTIC_COMMENT("make cursor invisible")},
   {"vs", 20		UNTIC_COMMENT("make cursor very visible")},
   {"wi", 133		UNTIC_COMMENT("current window is lines #1-#2 cols #3-#4")},
   {"xl", 361		UNTIC_COMMENT("Program function key #1 to type string #2 and show string #3")},
   {"", -1		UNTIC_COMMENT(NULL)}
};

static int compute_cap_offset (char *cap, SLterminfo_Type *t, Tgetstr_Map_Type *map, unsigned int max_ofs)
{
   char cha, chb;

   (void) t;
   cha = *cap++; chb = *cap;

   while (*map->name != 0)
     {
	if ((cha == *map->name) && (chb == *(map->name + 1)))
	  {
	     if (map->offset >= (int) max_ofs) return -1;
	     return map->offset;
	  }
	map++;
     }
   return -1;
}

char *_pSLtt_tigetstr (SLterminfo_Type *t, char *cap)
{
   int offset;

   if (t == NULL)
     return NULL;

   if (t->flags == SLTERMCAP) return tcap_getstr (cap, t);

   offset = compute_cap_offset (cap, t, Tgetstr_Map, t->num_string_offsets);
   if (offset < 0) return NULL;
   offset = make_integer (t->string_offsets + 2 * offset);
   if (offset < 0) return NULL;
   return t->string_table + offset;
}

static Tgetstr_Map_Type Tgetnum_Map[] =
{
   {"BT", 30		UNTIC_COMMENT("number of buttons on mouse")},
   {"Co", 13		UNTIC_COMMENT("maximum numbers of colors on screen")},
   {"MW", 12		UNTIC_COMMENT("maxumum number of defineable windows")},
   {"NC", 15		UNTIC_COMMENT("video attributes that can't be used with colors")},
   {"Nl", 8		UNTIC_COMMENT("number of labels on screen")},
   {"Ya", 16		UNTIC_COMMENT("numbers of bytes buffered before printing")},
   {"Yb", 17		UNTIC_COMMENT("spacing of pins vertically in pins per inch")},
   {"Yc", 18		UNTIC_COMMENT("spacing of dots horizontally in dots per inch")},
   {"Yd", 19		UNTIC_COMMENT("maximum value in micro_..._address")},
   {"Ye", 20		UNTIC_COMMENT("maximum value in parm_..._micro")},
   {"Yf", 21		UNTIC_COMMENT("character size when in micro mode")},
   {"Yg", 22		UNTIC_COMMENT("line size when in micro mode")},
   {"Yh", 23		UNTIC_COMMENT("numbers of pins in print-head")},
   {"Yi", 24		UNTIC_COMMENT("horizontal resolution in units per line")},
   {"Yj", 25		UNTIC_COMMENT("vertical resolution in units per line")},
   {"Yk", 26		UNTIC_COMMENT("horizontal resolution in units per inch")},
   {"Yl", 27		UNTIC_COMMENT("vertical resolution in units per inch")},
   {"Ym", 28		UNTIC_COMMENT("print rate in chars per second")},
   {"Yn", 29		UNTIC_COMMENT("character step size when in double wide mode")},
   {"Yo", 31		UNTIC_COMMENT("number of passed for each bit-image row")},
   {"Yp", 32		UNTIC_COMMENT("type of bit-image device")},
   {"co", 0		UNTIC_COMMENT("number of columns in aline")},
   {"dB", 36		UNTIC_COMMENT("padding required for ^H")},
   {"dC", 34		UNTIC_COMMENT("pad needed for CR")},
   {"dN", 35		UNTIC_COMMENT("pad needed for LF")},
   {"dT", 37		UNTIC_COMMENT("padding required for ^I")},
   {"it", 1		UNTIC_COMMENT("tabs initially every # spaces")},
   {"kn", 38		UNTIC_COMMENT("count of function keys")},
   {"lh", 9		UNTIC_COMMENT("rows in each label")},
   {"li", 2		UNTIC_COMMENT("number of lines on screen or page")},
   {"lm", 3		UNTIC_COMMENT("lines of memory if > line. 0 => varies")},
   {"lw", 10		UNTIC_COMMENT("columns in each label")},
   {"ma", 11		UNTIC_COMMENT("maximum combined attributes terminal can handle")},
   {"pa", 14		UNTIC_COMMENT("maximum number of color-pairs on the screen")},
   {"pb", 5		UNTIC_COMMENT("lowest baud rate where padding needed")},
   {"sg", 4		UNTIC_COMMENT("number of blank chars left by smso or rmso")},
   {"ug", 33		UNTIC_COMMENT("number of blanks left by ul")},
   {"vt", 6		UNTIC_COMMENT("virtual terminal number (CB/unix)")},
   {"ws", 7		UNTIC_COMMENT("columns in status line")},
   {"", -1		UNTIC_COMMENT(NULL)}
};

int _pSLtt_tigetnum (SLterminfo_Type *t, char *cap)
{
   int offset;

   if (t == NULL)
     return -1;

   if (t->flags == SLTERMCAP) return tcap_getnum (cap, t);

   offset = compute_cap_offset (cap, t, Tgetnum_Map, t->num_numbers);
   if (offset < 0) return -1;
   return make_integer (t->numbers + 2 * offset);
}

static Tgetstr_Map_Type Tgetflag_Map[] =
{
   {"5i", 22		UNTIC_COMMENT("printer won't echo on screen")},
   {"HC", 23		UNTIC_COMMENT("cursor is hard to see")},
   {"MT", 40		UNTIC_COMMENT("has meta key")},
   {"ND", 26		UNTIC_COMMENT("scrolling region is non-destructive")},
   {"NL", 41		UNTIC_COMMENT("move down with \n")},
   {"NP", 25		UNTIC_COMMENT("pad character does not exist")},
   {"NR", 24		UNTIC_COMMENT("smcup does not reverse rmcup")},
   {"YA", 30		UNTIC_COMMENT("only positive motion for hpa/mhpa caps")},
   {"YB", 31		UNTIC_COMMENT("using cr turns off micro mode")},
   {"YC", 32		UNTIC_COMMENT("printer needs operator to change character set")},
   {"YD", 33		UNTIC_COMMENT("only positive motion for vpa/mvpa caps")},
   {"YE", 34		UNTIC_COMMENT("printing in last column causes cr")},
   {"YF", 35		UNTIC_COMMENT("changing character pitch changes resolution")},
   {"YG", 36		UNTIC_COMMENT("changing line pitch changes resolution")},
   {"am", 1		UNTIC_COMMENT("terminal has automatic margins")},
   {"bs", 37		UNTIC_COMMENT("uses ^H to move left")},
   {"bw", 0		UNTIC_COMMENT("cub1 wraps from column 0 to last column")},
   {"cc", 27		UNTIC_COMMENT("terminal can re-define existing colors")},
   {"da", 11		UNTIC_COMMENT("display may be retained above the screen")},
   {"db", 12		UNTIC_COMMENT("display may be retained below the screen")},
   {"eo", 5		UNTIC_COMMENT("can erase overstrikes with a blank")},
   {"es", 16		UNTIC_COMMENT("escape can be used on the status line")},
   {"gn", 6		UNTIC_COMMENT("generic line type")},
   {"hc", 7		UNTIC_COMMENT("hardcopy terminal")},
   {"hl", 29		UNTIC_COMMENT("terminal uses only HLS color notation (tektronix)")},
   {"hs", 9		UNTIC_COMMENT("has extra status line")},
   {"hz", 18		UNTIC_COMMENT("can't print ~'s (hazeltine)")},
   {"in", 10		UNTIC_COMMENT("insert mode distinguishes nulls")},
   {"km", 8		UNTIC_COMMENT("Has a meta key, sets msb high")},
   {"mi", 13		UNTIC_COMMENT("safe to move while in insert mode")},
   {"ms", 14		UNTIC_COMMENT("safe to move while in standout mode")},
   {"nc", 39		UNTIC_COMMENT("no way to go to start of line")},
   {"ns", 38		UNTIC_COMMENT("crt cannot scroll")},
   {"nx", 21		UNTIC_COMMENT("padding won't work, xon/xoff required")},
   {"os", 15		UNTIC_COMMENT("terminal can overstrike")},
   {"pt", 42		UNTIC_COMMENT("has 8-char tabs invoked with ^I")},
   {"ul", 19		UNTIC_COMMENT("underline character overstrikes")},
   {"ut", 28		UNTIC_COMMENT("screen erased with background color")},
   {"xb", 2		UNTIC_COMMENT("beehive (f1=escape, f2=ctrl C)")},
   {"xn", 4		UNTIC_COMMENT("newline ignored after 80 cols (concept)")},
   {"xo", 20		UNTIC_COMMENT("terminal uses xon/xoff handshaking")},
   {"xr", 43		UNTIC_COMMENT("return clears the line")},
   {"xs", 3		UNTIC_COMMENT("standout not erased by overwriting (hp)")},
   {"xt", 17		UNTIC_COMMENT("tabs destructive, magic so char (t1061)")},
   {"", -1		UNTIC_COMMENT(NULL)}
};

int _pSLtt_tigetflag (SLterminfo_Type *t, char *cap)
{
   int offset;

   if (t == NULL) return -1;

   if (t->flags == SLTERMCAP) return tcap_getflag (cap, t);

   offset = compute_cap_offset (cap, t, Tgetflag_Map, t->boolean_section_size);

   if (offset < 0) return -1;
   return (int) *(t->boolean_flags + offset);
}

/* These are my termcap routines.  They only work with the TERMCAP environment
 * variable.  This variable must contain the termcap entry and NOT the file.
 */

static int tcap_getflag (char *cap, SLterminfo_Type *t)
{
   char a, b;
   char *f = (char *) t->boolean_flags;
   char *fmax;

   if (f == NULL) return 0;
   fmax = f + t->boolean_section_size;

   a = *cap;
   b = *(cap + 1);
   while (f < fmax)
     {
	if ((a == f[0]) && (b == f[1]))
	  return 1;
	f += 2;
     }
   return 0;
}

static char *tcap_get_cap (unsigned char *cap, unsigned char *caps, unsigned int len)
{
   unsigned char c0, c1;
   unsigned char *caps_max;

   c0 = cap[0];
   c1 = cap[1];

   if (caps == NULL) return NULL;
   caps_max = caps + len;
   while (caps < caps_max)
     {
	if ((c0 == caps[0]) && (c1 == caps[1]))
	  {
	     return (char *) caps + 3;
	  }
	caps += (int) caps[2];
     }
   return NULL;
}

static int tcap_getnum (char *cap, SLterminfo_Type *t)
{
   cap = tcap_get_cap ((unsigned char *) cap, t->numbers, t->num_numbers);
   if (cap == NULL) return -1;
   return atoi (cap);
}

static char *tcap_getstr (char *cap, SLterminfo_Type *t)
{
   return tcap_get_cap ((unsigned char *) cap, (unsigned char *) t->string_table, t->string_table_size);
}

static int tcap_extract_field (unsigned char *t0)
{
   register unsigned char ch, *t = t0;
   while (((ch = *t) != 0) && (ch != ':')) t++;
   if (ch == ':') return (int) (t - t0);
   return -1;
}

int SLtt_Try_Termcap = 1;
static int tcap_getent (char *term, SLterminfo_Type *ti)
{
   unsigned char *termcap, ch;
   unsigned char *buf, *b;
   unsigned char *t;
   int len;

   if (SLtt_Try_Termcap == 0) return -1;
#if 1
   /* XFREE86 xterm sets the TERMCAP environment variable to an invalid
    * value.  Specifically, it lacks the tc= string.
    */
   if (!strncmp (term, "xterm", 5))
     return -1;
#endif
   termcap = (unsigned char *) getenv ("TERMCAP");
   if ((termcap == NULL) || (*termcap == '/')) return -1;

   /* SUN Solaris 7&8 have bug in tset program under tcsh,
    * eval `tset -s -A -Q` sets value of TERMCAP to ":",
    *  under other shells it works fine.
    *  SUN was informed, they marked it as duplicate of bug 4086585
    *  but didn't care to fix it... <mikkopa@cs.tut.fi> 
    */
   if ((termcap[0] == ':') && (termcap[1] == 0))
     return -1;
   
   
   /* We have a termcap so lets use it provided it does not have a reference
    * to another terminal via tc=.  In that case, use terminfo.  The alternative
    * would be to parse the termcap file which I do not want to do right now.
    * Besides, this is a terminfo based system and if the termcap were parsed
    * terminfo would almost never get a chance to run.  In addition, the tc=
    * thing should not occur if tset is used to set the termcap entry.
    */
   t = termcap;
   while ((len = tcap_extract_field (t)) != -1)
     {
	if ((len > 3) && (t[0] == 't') && (t[1] == 'c') && (t[2] == '='))
	  return -1;
	t += (len + 1);
     }

   /* malloc some extra space just in case it is needed. */
   len = strlen ((char *) termcap) + 256;
   if (NULL == (buf = (unsigned char *) SLmalloc ((unsigned int) len))) 
     return -1;

   b = buf;

   /* The beginning of the termcap entry contains the names of the entry.
    * It is terminated by a colon.
    */

   ti->terminal_names = (char *) b;
   t = termcap;
   len = tcap_extract_field (t);
   if (len < 0)
     {
	SLfree ((char *)buf);
	return -1;
     }
   strncpy ((char *) b, (char *) t, (unsigned int) len);
   b[len] = 0;
   b += len + 1;
   ti->name_section_size = len;

   /* Now, we are really at the start of the termcap entries.  Point the
    * termcap variable here since we want to refer to this a number of times.
    */
   termcap = t + (len + 1);

   /* Process strings first. */
   ti->string_table = (char *) b;
   t = termcap;
   while (-1 != (len = tcap_extract_field (t)))
     {
	unsigned char *b1;
	unsigned char *tmax;

	/* We are looking for: XX=something */
	if ((len < 4) || (t[2] != '=') || (*t == '.'))
	  {
	     t += len + 1;
	     continue;
	  }
	tmax = t + len;
	b1 = b;

	while (t < tmax)
	  {
	     ch = *t++;
	     if ((ch == '\\') && (t < tmax))
	       {
		  SLwchar_Type wch;

		  t = (unsigned char *) _pSLexpand_escaped_char ((char *) t, &wch, NULL);
		  if (t == NULL)
		    {
		       SLfree ((char *)buf);
		       return -1;
		    }
		  ch = (char) wch;
	       }
	     else if ((ch == '^') && (t < tmax))
	       {
		  ch = *t++;
		  if (ch == '?') ch = 127;
		  else ch = (ch | 0x20) - ('a' - 1);
	       }
	     *b++ = ch;
	  }
	/* Null terminate it. */
	*b++ = 0;
	len = (int) (b - b1);
	b1[2] = (unsigned char) len;    /* replace the = by the length */
	/* skip colon to next field. */
	t++;
     }
   ti->string_table_size = (int) (b - (unsigned char *) ti->string_table);

   /* Now process the numbers. */

   t = termcap;
   ti->numbers = b;
   while (-1 != (len = tcap_extract_field (t)))
     {
	unsigned char *b1;
	unsigned char *tmax;

	/* We are looking for: XX#NUMBER */
	if ((len < 4) || (t[2] != '#') || (*t == '.'))
	  {
	     t += len + 1;
	     continue;
	  }
	tmax = t + len;
	b1 = b;

	while (t < tmax)
	  {
	     *b++ = *t++;
	  }
	/* Null terminate it. */
	*b++ = 0;
	len = (int) (b - b1);
	b1[2] = (unsigned char) len;    /* replace the # by the length */
	t++;
     }
   ti->num_numbers = (int) (b - ti->numbers);

   /* Now process the flags. */
   t = termcap;
   ti->boolean_flags = b;
   while (-1 != (len = tcap_extract_field (t)))
     {
	/* We are looking for: XX#NUMBER */
	if ((len != 2) || (*t == '.') || (*t <= ' '))
	  {
	     t += len + 1;
	     continue;
	  }
	b[0] = t[0];
	b[1] = t[1];
	t += 3;
	b += 2;
     }
   ti->boolean_section_size = (int) (b - ti->boolean_flags);
   ti->flags = SLTERMCAP;
   return 0;
}


/* These routines are provided only for backward binary compatability.
 * They will vanish in V2.x
 */
char *SLtt_tigetent (char *s)
{
   return (char *) _pSLtt_tigetent (s);
}

extern char *SLtt_tigetstr (char *s, char **p)
{
   if (p == NULL)
     return NULL;
   return _pSLtt_tigetstr ((SLterminfo_Type *) *p, s);
}

extern int SLtt_tigetnum (char *s, char **p)
{
   if (p == NULL)
     return -1;
   return _pSLtt_tigetnum ((SLterminfo_Type *) *p, s);
}


