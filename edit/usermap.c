/* cooledit.bindings file parser

   Authors: 2005 Vitja Makarov

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
*/

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mhl/types.h>
#include <mhl/memory.h>
#include <mhl/string.h>

#include "../src/global.h"

#include "edit.h"
#include "edit-widget.h"
#include "editcmddef.h"		/* list of commands */
#include "usermap.h"
#include "../src/key.h"		/* KEY_M_* */
#include "../src/tty.h"		/* keys */
#include "../src/wtools.h"
#include "../src/widget.h"	/* buttonbar_redraw() */

typedef struct NameMap {
    const char *name;
    int val;
} name_map_t;

typedef struct Config {
    time_t mtime;	/* mtime at the moment we read config file */
    GArray *keymap;
    GArray *ext_keymap;
    gchar *labels[10];
} config_t;

typedef struct Command {
    const char *name;
    bool (*handler) (config_t *cfg, int argc, char *argv[]);
} command_t;

static char error_msg[200] = "Nobody see this";

static const name_map_t key_names[] = {
    {"backspace", KEY_BACKSPACE},
    {"end", KEY_END},
    {"up", KEY_UP},
    {"down", KEY_DOWN},
    {"left", KEY_LEFT},
    {"right", KEY_RIGHT},
    {"home", KEY_HOME},
    {"a1", KEY_A1},
    {"c1", KEY_C1},
    {"npage", KEY_NPAGE},
    {"ppage", KEY_PPAGE},
    {"ic", KEY_IC},
    {"enter", KEY_ENTER},
    {"dc", KEY_DC},
    {"scancel", KEY_SCANCEL},
    {"btab", KEY_BTAB},
    {"f11", KEY_F(11)},
    {"f12", KEY_F(12)},
    {"f13", KEY_F(13)},
    {"f14", KEY_F(14)},
    {"f15", KEY_F(15)},
    {"f16", KEY_F(16)},
    {"f17", KEY_F(17)},
    {"f18", KEY_F(18)},
    {"f19", KEY_F(19)},
    {"f20", KEY_F(20)},
    {"tab", '\t'},
    {"return", '\n'},
    {NULL, 0}
};

static const name_map_t command_names[] = {
    {"No-Command", CK_Ignore_Key},
    {"Ignore-Key", CK_Ignore_Key},
    {"BackSpace", CK_BackSpace},
    {"Delete", CK_Delete},
    {"Enter", CK_Enter},
    {"Page-Up", CK_Page_Up},
    {"Page-Down", CK_Page_Down},
    {"Left", CK_Left},
    {"Right", CK_Right},
    {"Word-Left", CK_Word_Left},
    {"Word-Right", CK_Word_Right},
    {"Up", CK_Up},
    {"Down", CK_Down},
    {"Home", CK_Home},
    {"End", CK_End},
    {"Tab", CK_Tab},
    {"Undo", CK_Undo},
    {"Beginning-Of-Text", CK_Beginning_Of_Text},
    {"End-Of-Text", CK_End_Of_Text},
    {"Scroll-Up", CK_Scroll_Up},
    {"Scroll-Down", CK_Scroll_Down},
    {"Return", CK_Return},
    {"Begin-Page", CK_Begin_Page},
    {"End-Page", CK_End_Page},
    {"Delete-Word-Left", CK_Delete_Word_Left},
    {"Delete-Word-Right", CK_Delete_Word_Right},
    {"Paragraph-Up", CK_Paragraph_Up},
    {"Paragraph-Down", CK_Paragraph_Down},
    {"Save", CK_Save},
    {"Load", CK_Load},
    {"New", CK_New},
    {"Save-as", CK_Save_As},
    {"Mark", CK_Mark},
    {"Copy", CK_Copy},
    {"Move", CK_Move},
    {"Remove", CK_Remove},
    {"Unmark", CK_Unmark},
    {"Save-Block", CK_Save_Block},
    {"Column-Mark", CK_Column_Mark},
    {"Find", CK_Find},
    {"Find-Again", CK_Find_Again},
    {"Replace", CK_Replace},
    {"Replace-Again", CK_Replace_Again},
    {"Complete-Word", CK_Complete_Word},
    {"Debug-Start", CK_Debug_Start},
    {"Debug-Stop", CK_Debug_Stop},
    {"Debug-Toggle-Break", CK_Debug_Toggle_Break},
    {"Debug-Clear", CK_Debug_Clear},
    {"Debug-Next", CK_Debug_Next},
    {"Debug-Step", CK_Debug_Step},
    {"Debug-Back-Trace", CK_Debug_Back_Trace},
    {"Debug-Continue", CK_Debug_Continue},
    {"Debug-Enter-Command", CK_Debug_Enter_Command},
    {"Debug-Until-Curser", CK_Debug_Until_Curser},
    {"Insert-File", CK_Insert_File},
    {"Exit", CK_Exit},
    {"Toggle-Insert", CK_Toggle_Insert},
    {"Help", CK_Help},
    {"Date", CK_Date},
    {"Refresh", CK_Refresh},
    {"Goto", CK_Goto},
    {"Delete-Line", CK_Delete_Line},
    {"Delete-To-Line-End", CK_Delete_To_Line_End},
    {"Delete-To-Line-Begin", CK_Delete_To_Line_Begin},
    {"Man-Page", CK_Man_Page},
    {"Sort", CK_Sort},
    {"Mail", CK_Mail},
    {"Cancel", CK_Cancel},
    {"Complete", CK_Complete},
    {"Paragraph-Format", CK_Paragraph_Format},
    {"Util", CK_Util},
    {"Type-Load-Python", CK_Type_Load_Python},
    {"Find-File", CK_Find_File},
    {"Ctags", CK_Ctags},
    {"Match-Bracket", CK_Match_Bracket},
    {"Terminal", CK_Terminal},
    {"Terminal-App", CK_Terminal_App},
    {"ExtCmd", CK_ExtCmd},
    {"User-Menu", CK_User_Menu},
    {"Save-Desktop", CK_Save_Desktop},
    {"New-Window", CK_New_Window},
    {"Cycle", CK_Cycle},
    {"Menu", CK_Menu},
    {"Save-And-Quit", CK_Save_And_Quit},
    {"Run-Another", CK_Run_Another},
    {"Check-Save-And-Quit", CK_Check_Save_And_Quit},
    {"Maximize", CK_Maximize},
    {"Begin-Record-Macro", CK_Begin_Record_Macro},
    {"End-Record-Macro", CK_End_Record_Macro},
    {"Delete-Macro", CK_Delete_Macro},
    {"Toggle-Bookmark", CK_Toggle_Bookmark},
    {"Flush-Bookmarks", CK_Flush_Bookmarks},
    {"Next-Bookmark", CK_Next_Bookmark},
    {"Prev-Bookmark", CK_Prev_Bookmark},
    {"Page-Up-Highlight", CK_Page_Up_Highlight},
    {"Page-Down-Highlight", CK_Page_Down_Highlight},
    {"Left-Highlight", CK_Left_Highlight},
    {"Right-Highlight", CK_Right_Highlight},
    {"Word-Left-Highlight", CK_Word_Left_Highlight},
    {"Word-Right-Highlight", CK_Word_Right_Highlight},
    {"Up-Highlight", CK_Up_Highlight},
    {"Down-Highlight", CK_Down_Highlight},
    {"Home-Highlight", CK_Home_Highlight},
    {"End-Highlight", CK_End_Highlight},
    {"Beginning-Of-Text-Highlight", CK_Beginning_Of_Text_Highlight},
    {"End-Of-Text_Highlight", CK_End_Of_Text_Highlight},
    {"Begin-Page-Highlight", CK_Begin_Page_Highlight},
    {"End-Page-Highlight", CK_End_Page_Highlight},
    {"Scroll-Up-Highlight", CK_Scroll_Up_Highlight},
    {"Scroll-Down-Highlight", CK_Scroll_Down_Highlight},
    {"Paragraph-Up-Highlight", CK_Paragraph_Up_Highlight},
    {"Paragraph-Down-Highlight", CK_Paragraph_Down_Highlight},
    {"XStore", CK_XStore},
    {"XCut", CK_XCut},
    {"XPaste", CK_XPaste},
    {"Selection-History", CK_Selection_History},
    {"Shell", CK_Shell},
    {"Select-Codepage", CK_Select_Codepage},
    {"Insert-Literal", CK_Insert_Literal},
    {"Execute-Macro", CK_Execute_Macro},
    {"Begin-or-End-Macro", CK_Begin_End_Macro},
    {"Ext-mode", CK_Ext_Mode},
#if 0
    {"Focus-Next", CK_Focus_Next},
    {"Focus-Prev", CK_Focus_Prev},
    {"Height-Inc", CK_Height_Inc},
    {"Height-Dec", CK_Height_Dec},
    {"Make", CK_Make},
    {"Error-Next", CK_Error_Next},
    {"Error-Prev", CK_Error_Prev},
#endif
    {NULL, 0}
};

static void
cfg_free_maps(config_t *cfg)
{
    int i;

    if (cfg->keymap)
	g_array_free(cfg->keymap, TRUE);
    cfg->keymap = NULL;
    if (cfg->ext_keymap)
	g_array_free(cfg->ext_keymap, TRUE);
    cfg->ext_keymap = NULL;

    for (i = 0; i < 10; i++)
	g_free(cfg->labels[i]);
}

/* Returns an array containing the words in str.  WARNING: As long as
 * the result is used, str[...] must be valid memory.  This function
 * modifies str[...] and uses it, so the caller should not use it until
 * g_ptr_array_free() is called for the returned result.
 */
static GPtrArray *
split_line(char *str)
{
    bool inside_quote = false;
    int move = 0;
    GPtrArray *args;    

    args = g_ptr_array_new();

    /* skip spaces */
    while (isspace((unsigned char) *str))
	str++;
    
    g_ptr_array_add(args, str);

    for (;; str++) {
	switch (*str) {
	case '#':		/* cut off comments */
	    if (inside_quote)
		break;
	    /* FALLTHROUGH */

	case '\n':		/* end of line */
	case '\r':
	case '\0':
	    if (str == g_ptr_array_index(args, args->len - 1))
		g_ptr_array_remove_index(args, args->len - 1);
	    else
		*(str - move) = '\0';
	    return args;

	case '"':		/* quote */
	case '\'':
	    inside_quote = !inside_quote;
	    move++;
	    continue;

	case ' ':		/* spaces */
	case '\t':
	    if (inside_quote)
		break;

	    *(str++ - move) = '\0';
	    move = 0;

	    /* skip spaces */
	    while (isspace((unsigned char) *str))
		str++;

	    g_ptr_array_add(args, str--);
	    break;

	case '\\':
	    switch (*(++str)) {
	    case 'n':
		*str = '\n';
		break;
	    case 'r':
		*str = '\r';
		break;
	    case 't':
		*str = '\t';
		break;
	    }
	    move++;
	    break;
	}
	if (move != 0)
	    *(str - move) = *str;
    }
    /* never be here */
}

static void
keymap_add(GArray *keymap, int key, int cmd)
{
    edit_key_map_type new_one, *map;
    guint i;

    map = &(g_array_index(keymap, edit_key_map_type, 0));
    for (i = 0; i < keymap->len; i++) {
	if (map[i].key == key) {
	    map[i].command = cmd;
	    return;
	}
    }

    new_one.key = key;
    new_one.command = cmd;
    g_array_append_val(keymap, new_one);
}

/* bind <key> <command> */
static bool
cmd_bind(config_t *cfg, int argc, char *argv[])
{
    char *keyname, *command;
    const name_map_t *key = key_names;
    const name_map_t *cmd = command_names;
    int mod = 0, k = -1, m = 0;

    if (argc != 3) {
	snprintf(error_msg, sizeof(error_msg),
		 _("bind: Wrong argument number, bind <key> <command>"));
	return false;
    }

    keyname = argv[1];
    command = argv[2];

    while (*keyname) {
	switch (*keyname++) {
	case 'C':
	    m = KEY_M_CTRL;
	    continue;
	case 'M':
	case 'A':
	    m = KEY_M_ALT;
	    continue;
	case 'S':
	    m = KEY_M_SHIFT;
	    continue;
	case '-':
	    if (!m) {		/* incorrect key */
		snprintf(error_msg, sizeof(error_msg),
			 _("bind: Bad key value `%s'"), keyname);
		return false;
	    }
	    mod |= m;
	    m = 0;
	    continue;
	}
	keyname--;
	break;
    }

    /* no key */
    if (keyname[0] == '\0') {
	snprintf(error_msg, sizeof(error_msg), _("bind: Ehh...no key?"));
	return false;
    }

    /* ordinary key */
    if (keyname[1] == '\0') {
	k = keyname[0];
    } else { 
	while (key->name && strcasecmp(key->name, keyname) != 0)
	    key++;
    }

    if (k < 0 && !key->name) {
	snprintf(error_msg, sizeof(error_msg),
		 _("bind: Unknown key: `%s'"), keyname);
	return false;
    }

    while (cmd->name && strcasecmp(cmd->name, command) != 0)
	cmd++;

    if (!cmd->name) {
	snprintf(error_msg, sizeof(error_msg),
		 _("bind: Unknown command: `%s'"), command);
	return false;
    }

    if (mod & KEY_M_CTRL) {
	if (k < 256)
	    k = XCTRL(k);
	else
	    k |= KEY_M_CTRL;
    }

    if (mod & KEY_M_ALT)
	k |= KEY_M_ALT;

    if (mod & KEY_M_SHIFT)
	k |= KEY_M_SHIFT;

    if (!strcasecmp("bind-ext", argv[0]))
	keymap_add(cfg->ext_keymap, k, cmd->val);
    else
	keymap_add(cfg->keymap, k, cmd->val);

    return true;
}

#if 0
#define CMD_F(x)				\
    static void cmd_F ## x (WEdit * edit)	\
    {						\
	send_message ((Widget *) edit, WIDGET_KEY, KEY_F (x));	\
    }

CMD_F(1)
CMD_F(2)
CMD_F(3)
CMD_F(4)
CMD_F(5)
CMD_F(6)
CMD_F(7)
CMD_F(8)
CMD_F(9)
CMD_F(10)

void (*cmd_Fx[]) (WEdit *) = {
    cmd_F1, cmd_F2, cmd_F3, cmd_F4, cmd_F5, 
    cmd_F6, cmd_F7, cmd_F8, cmd_F9, cmd_F10
} ;

/* move me! */
static void edit_my_define (Dlg_head * h, int idx, const char *text,
			    void (*fn) (WEdit *), WEdit * edit)
{
    /* function-cast ok */
    buttonbar_set_label_data (h, idx, text, (buttonbarfn) fn, edit);
}
#endif

/* label <number> <command> <label> */
static bool
cmd_label(config_t *cfg, int argc, char *argv[])
{
    const name_map_t *cmd = command_names;
    const char *command, *label;
    int fn;

    if (argc != 4) {
	snprintf(error_msg, sizeof(error_msg),
		 _("%s: Syntax: %s <n> <command> <label>"), 
		argv[0], argv[0]);
	return false;
    }
    
    fn	= strtol(argv[1], NULL, 0);
    command = argv[2];
    label = argv[3];

    while (cmd->name && strcasecmp(cmd->name, command) != 0)
	cmd++;

    if (!cmd->name) {
	snprintf(error_msg, sizeof(error_msg),
		 _("%s: Unknown command: `%s'"), argv[0], command);
	return false;
    }
    
    if (fn < 1 || fn > 10) {
	snprintf(error_msg, sizeof(error_msg),
		 _("%s: fn should be 1-10"), argv[0]);
	return false;
    }

    keymap_add(cfg->keymap, KEY_F(fn), cmd->val);
    cfg->labels[fn - 1] = g_strdup(label);

    return true;
}


static bool
parse_file(config_t *cfg, const char *file, const command_t *cmd)
{
    char buf[200];
    int line = 0;

    FILE *fp = fopen(file, "r");

    if (!fp) {
	snprintf(error_msg, sizeof(error_msg), _("%s: fopen(): %s"),
		 file, strerror(errno));
	return false;
    }

    while (fgets(buf, sizeof(buf), fp)) {
    	const command_t *c = cmd;
	GPtrArray *args;
	char **argv;

	line++;

	args = split_line(buf);
	argv = (char **) args->pdata;

	if (args->len == 0) {
	    g_ptr_array_free(args, TRUE);
	    continue;
	}

	while (c->name != NULL && strcasecmp(c->name, argv[0]) != 0)
	    c++;

	if (c->name == NULL) {
	    snprintf(error_msg, sizeof(error_msg),
		     _("%s:%d: unknown command `%s'"), file, line,
		     argv[0]);
	    g_ptr_array_free(args, TRUE);
	    fclose(fp);
	    return false;
	}

	if (!(c->handler(cfg, args->len, argv))) {
	    char *ss = g_strdup(error_msg);
	    snprintf(error_msg, sizeof(error_msg),
			 _("%s:%d: %s"), file, line, ss);
	    g_free(ss);
	    g_ptr_array_free(args, TRUE);
	    fclose(fp);
	    return false;
	}
	g_ptr_array_free(args, TRUE);
    }

    fclose(fp);
    return true;
}

static bool
load_user_keymap(config_t *cfg, const char *file)
{
    const command_t cmd[] = {
	{"bind", cmd_bind},
	{"bind-ext", cmd_bind},
	{"label", cmd_label},
	{0, 0}
    };

    cfg->keymap = g_array_new(TRUE, FALSE, sizeof(edit_key_map_type));
    cfg->ext_keymap = g_array_new(TRUE, FALSE, sizeof(edit_key_map_type));

    if (!parse_file(cfg, file, cmd)) {
	return false;
    }

    return true;
}

bool
edit_load_user_map(WEdit *edit)
{
    static config_t cfg;
    config_t new_cfg;
    char *file;
    struct stat s;

    if (edit_key_emulation != EDIT_KEY_EMULATION_USER)
	return true;

    file = mhl_str_dir_plus_file(home_dir, MC_USERMAP);

    if (stat(file, &s) < 0) {
	char *msg = g_strdup_printf(_("%s not found!"), file);
	edit_error_dialog(_("Error"), msg);
	g_free(msg);
	g_free(file);
	return false;
    }

    if (s.st_mtime != cfg.mtime) {
	memset(&new_cfg, 0, sizeof(new_cfg));
	new_cfg.mtime = s.st_mtime;

	if (!load_user_keymap(&new_cfg, file)) {
	    edit_error_dialog(_("Error"), error_msg);
	    cfg_free_maps(&new_cfg);
	    g_free(file);
	    return false;
	} else {
	    cfg_free_maps(&cfg);
	    cfg = new_cfg;
	}
    }

    edit->user_map = (edit_key_map_type *) cfg.keymap->data;
    edit->ext_map = (edit_key_map_type *) cfg.ext_keymap->data;
    memcpy(edit->labels, cfg.labels, sizeof(edit->labels));

    g_free(file);

    return true;
}
