/* File management GUI for the text mode edition
 *
 * Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
 * 2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.
 *
 * Written by:    2005        Vitja Makarov
 *                2009        Ilia Maslakov
 *
 * The copy code was based in GNU's cp, and was written by:
 * Torbjorn Granlund, David MacKenzie, and Jim Meyering.
 *
 * The move code was based in GNU's mv, and was written by:
 * Mike Parker and David MacKenzie.
 *
 * Janne Kukonlehto added much error recovery to them for being used
 * in an interactive program.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "global.h"

#include "cmddef.h"		/* list of commands */
#include "tty/win.h"
#include "tty/key.h"		/* KEY_M_ */
#include "tty/tty.h"		/* keys */
#include "wtools.h"
#include "keybind.h"

static const name_key_map_t command_names[] = {
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
    {"Toggle-Line-State", CK_Toggle_Line_State},
#if 0
    {"Focus-Next", CK_Focus_Next},
    {"Focus-Prev", CK_Focus_Prev},
    {"Height-Inc", CK_Height_Inc},
    {"Height-Dec", CK_Height_Dec},
    {"Make", CK_Make},
    {"Error-Next", CK_Error_Next},
    {"Error-Prev", CK_Error_Prev},
#endif
    {"ChmodCmd", CK_ChmodCmd},
    {"ChownAdvancedCmd", CK_ChownAdvancedCmd},
    {"ChownCmd", CK_ChownCmd},
    {"CompareDirsCmd", CK_CompareDirsCmd},
    {"ConfigureBox", CK_ConfigureBox},
    {"ConfigureVfs", CK_ConfigureVfs},
    {"ConfirmBox", CK_ConfirmBox},
    {"CopyCmd", CK_CopyCmd},
    {"DeleteCmd", CK_DeleteCmd},
    {"DirsizesCmd", CK_DirsizesCmd},
    {"DisplayBitsBox", CK_DisplayBitsBox},
    {"EditCmd", CK_EditCmd},
    {"EditMcMenuCmd", CK_EditMcMenuCmd},
    {"EditSymlinkCmd", CK_EditSymlinkCmd},
    {"EditSyntaxCmd", CK_EditSyntaxCmd},
    {"EditUserMenuCmd", CK_EditUserMenuCmd},
    {"ExtCmd", CK_ExtCmd},
    {"ExternalPanelize", CK_ExternalPanelize},
    {"FilterCmd", CK_FilterCmd},
    {"FilteredViewCmd", CK_FilteredViewCmd},
    {"FindCmd", CK_FindCmd},
    {"FishlinkCmd", CK_FishlinkCmd},
    {"FtplinkCmd", CK_FtplinkCmd},
    {"HistoryCmd", CK_HistoryCmd},
    {"InfoCmd", CK_InfoCmd},
    {"JobsCmd", CK_JobsCmd},
    {"LayoutCmd", CK_LayoutCmd},
    {"LearnKeys", CK_LearnKeys},
    {"LinkCmd", CK_LinkCmd},
    {"ListingCmd", CK_ListingCmd},
    {"MkdirCmd", CK_MkdirCmd},
    {"QuickCdCmd", CK_QuickCdCmd},
    {"QuickChdirCmd", CK_QuickChdirCmd},
    {"QuickViewCmd", CK_QuickViewCmd},
    {"QuietQuitCmd", CK_QuietQuitCmd},
    {"RenCmd", CK_RenCmd},
    {"RereadCmd", CK_RereadCmd},
    {"ReselectVfs", CK_ReselectVfs},
    {"ReverseSelectionCmd", CK_ReverseSelectionCmd},
    {"SaveSetupCmd", CK_SaveSetupCmd},
    {"SelectCmd", CK_SelectCmd},
    {"SwapCmd", CK_SwapCmd},
    {"SymlinkCmd", CK_SymlinkCmd},
    {"TreeCmd", CK_TreeCmd},
    {"UndeleteCmd", CK_UndeleteCmd},
    {"UnselectCmd", CK_UnselectCmd},
    {"UserFileMenuCmd", CK_UserFileMenuCmd},
    {"ViewCmd", CK_ViewCmd},
    {"ViewFileCmd", CK_ViewFileCmd},
    {"ViewOtherCmd", CK_ViewOtherCmd},

    {"CopyCurrentReadlink",        CK_CopyCurrentReadlink },
    {"CopyOtherReadlink",          CK_CopyOtherReadlink },
    {"AddHotlist",                 CK_AddHotlist },
    {"StartExtCmd",                CK_StartExtCmd },
    {"QuitCmd",                    CK_QuitCmd },
    {"CopyOtherTarget",            CK_CopyOtherTarget },
    {"CopyOthertReadlink",         CK_CopyOthertReadlink },

    /* panel */
    { "PanelChdirOtherPanel",      CK_PanelChdirOtherPanel },
    { "PanelChdirToReadlink",      CK_PanelChdirToReadlink },
    { "PanelCopyLocal",            CK_PanelCmdCopyLocal },
    { "PanelDeleteLocal",          CK_PanelCmdDeleteLocal },
    { "PanelDoEnter",              CK_PanelCmdDoEnter },
    { "PanelEditNew",              CK_PanelCmdEditNew },
    { "PanelRenameLocal",          CK_PanelCmdRenameLocal },
    { "PanelReverseSelection",     CK_PanelCmdReverseSelection },
    { "PanelSelect",               CK_PanelCmdSelect },
    { "PanelUnselect",             CK_PanelCmdUnselect },
    { "PanelViewSimple",           CK_PanelCmdViewSimple },
    { "PanelCtrlNextPage",         CK_PanelCtrlNextPage },
    { "PanelCtrlPrevPage",         CK_PanelCtrlPrevPage },
    { "PanelDirectoryHistoryList", CK_PanelDirectoryHistoryList },
    { "PanelDirectoryHistoryNext", CK_PanelDirectoryHistoryNext },
    { "PanelDirectoryHistoryPrev", CK_PanelDirectoryHistoryPrev },
    { "PanelGotoBottomFile",       CK_PanelGotoBottomFile },
    { "PanelGotoMiddleFile",       CK_PanelGotoMiddleFile },
    { "PanelGotoTopFile",          CK_PanelGotoTopFile },
    { "PanelMarkFile",             CK_PanelMarkFile },
    { "PanelMoveUp",               CK_PanelMoveUp },
    { "PanelMoveDown",             CK_PanelMoveDown },
    { "PanelMoveLeft",             CK_PanelMoveLeft },
    { "PanelMoveRight",            CK_PanelMoveRight },
    { "PanelMoveEnd",              CK_PanelMoveEnd },
    { "PanelMoveHome",             CK_PanelMoveHome },
    { "PanelNextPage",             CK_PanelNextPage },
    { "PanelPrevPage",             CK_PanelPrevPage },
    { "PanelSetPanelEncoding",     CK_PanelSetPanelEncoding },
    { "PanelStartSearch",          CK_PanelStartSearch },
    { "PanelSyncOtherPanel",       CK_PanelSyncOtherPanel },

    {NULL, 0}
};

int lookup_action (char *keyname)
{
    int i;

    for (i = 0; command_names [i].name; i++){
        if (!str_casecmp (command_names [i].name, keyname))
            return command_names [i].val;
    }
    return 0;
}

void
keymap_add(GArray *keymap, int key, int cmd)
{
    global_key_map_t new_one, *map;
    guint i;
    map = &(g_array_index(keymap, global_key_map_t, 0));

    for (i = 0; i < keymap->len; i++) {
	if (map[i].key == key) {
	    map[i].command = cmd;
	    return;
	}
    }
    if (key !=0 && cmd !=0) {
        new_one.key = key;
        new_one.command = cmd;
        g_array_append_val(keymap, new_one);
    }

}

void
keybind_cmd_bind(GArray *keymap, char *keybind, int action)
{
    keymap_add(keymap, lookup_key(keybind), action);
}
