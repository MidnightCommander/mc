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

#include "cmddef.h"		/* CK_ cmd name const */
#include "tty/win.h"
#include "tty/key.h"		/* KEY_M_ */
#include "tty/tty.h"		/* keys */
#include "wtools.h"
#include "keybind.h"

static const name_key_map_t command_names[] = {
    { "EditNoCommand",                     CK_Ignore_Key },
    { "EditIgnoreKey",                     CK_Ignore_Key },
    { "EditBackSpace",                     CK_BackSpace },
    { "EditDelete",                        CK_Delete },
    { "EditEnter",                         CK_Enter },
    { "EditPageUp",                        CK_Page_Up },
    { "EditPageDown",                      CK_Page_Down },
    { "EditLeft",                          CK_Left },
    { "EditRight",                         CK_Right },
    { "EditWordLeft",                      CK_Word_Left },
    { "EditWordRight",                     CK_Word_Right },
    { "EditUp",                            CK_Up },
    { "EditDown",                          CK_Down },
    { "EditHome",                          CK_Home },
    { "EditEnd",                           CK_End },
    { "EditTab",                           CK_Tab },
    { "EditUndo",                          CK_Undo },
    { "EditBeginningOfText",               CK_Beginning_Of_Text },
    { "EditEndOfText",                     CK_End_Of_Text },
    { "EditScrollUp",                      CK_Scroll_Up },
    { "EditScrollDown",                    CK_Scroll_Down },
    { "EditReturn",                        CK_Return },
    { "EditBeginPage",                     CK_Begin_Page },
    { "EditEndPage",                       CK_End_Page },
    { "EditDeleteWordLeft",                CK_Delete_Word_Left },
    { "EditDeleteWordRight",               CK_Delete_Word_Right },
    { "EditParagraphUp",                   CK_Paragraph_Up },
    { "EditParagraphDown",                 CK_Paragraph_Down },
    { "EditSave",                          CK_Save },
    { "EditLoad",                          CK_Load },
    { "EditNew",                           CK_New },
    { "EditSaveas",                        CK_Save_As },
    { "EditMark",                          CK_Mark },
    { "EditCopy",                          CK_Copy },
    { "EditMove",                          CK_Move },
    { "EditRemove",                        CK_Remove },
    { "EditUnmark",                        CK_Unmark },
    { "EditSaveBlock",                     CK_Save_Block },
    { "EditColumnMark",                    CK_Column_Mark },
    { "EditFind",                          CK_Find },
    { "EditFindAgain",                     CK_Find_Again },
    { "EditReplace",                       CK_Replace },
    { "EditReplaceAgain",                  CK_Replace_Again },
    { "EditCompleteWord",                  CK_Complete_Word },
    { "EditDebugStart",                    CK_Debug_Start },
    { "EditDebugStop",                     CK_Debug_Stop },
    { "EditDebugToggleBreak",              CK_Debug_Toggle_Break },
    { "EditDebugClear",                    CK_Debug_Clear },
    { "EditDebugNext",                     CK_Debug_Next },
    { "EditDebugStep",                     CK_Debug_Step },
    { "EditDebugBackTrace",                CK_Debug_Back_Trace },
    { "EditDebugContinue",                 CK_Debug_Continue },
    { "EditDebugEnterCommand",             CK_Debug_Enter_Command },
    { "EditDebugUntilCurser",              CK_Debug_Until_Curser },
    { "EditInsertFile",                    CK_Insert_File },
    { "EditExit",                          CK_Exit },
    { "EditToggleInsert",                  CK_Toggle_Insert },
    { "EditHelp",                          CK_Help },
    { "EditDate",                          CK_Date },
    { "EditRefresh",                       CK_Refresh },
    { "EditGoto",                          CK_Goto },
    { "EditDeleteLine",                    CK_Delete_Line },
    { "EditDeleteToLineEnd",               CK_Delete_To_Line_End },
    { "EditDeleteToLineBegin",             CK_Delete_To_Line_Begin },
    { "EditManPage",                       CK_Man_Page },
    { "EditSort",                          CK_Sort },
    { "EditMail",                          CK_Mail },
    { "EditCancel",                        CK_Cancel },
    { "EditComplete",                      CK_Complete },
    { "EditParagraphFormat",               CK_Paragraph_Format },
    { "EditUtil",                          CK_Util },
    { "EditTypeLoadPython",                CK_Type_Load_Python },
    { "EditFindFile",                      CK_Find_File },
    { "EditCtags",                         CK_Ctags },
    { "EditMatchBracket",                  CK_Match_Bracket },
    { "EditTerminal",                      CK_Terminal },
    { "EditTerminalApp",                   CK_Terminal_App },
    { "EditExtCmd",                        CK_ExtCmd },
    { "EditUserMenu",                      CK_User_Menu },
    { "EditSaveDesktop",                   CK_Save_Desktop },
    { "EditNewWindow",                     CK_New_Window },
    { "EditCycle",                         CK_Cycle },
    { "EditMenu",                          CK_Menu },
    { "EditSaveAndQuit",                   CK_Save_And_Quit },
    { "EditRunAnother",                    CK_Run_Another },
    { "EditCheckSaveAndQuit",              CK_Check_Save_And_Quit },
    { "EditMaximize",                      CK_Maximize },
    { "EditBeginRecordMacro",              CK_Begin_Record_Macro },
    { "EditEndRecordMacro",                CK_End_Record_Macro },
    { "EditDeleteMacro",                   CK_Delete_Macro },
    { "EditToggleBookmark",                CK_Toggle_Bookmark },
    { "EditFlushBookmarks",                CK_Flush_Bookmarks },
    { "EditNextBookmark",                  CK_Next_Bookmark },
    { "EditPrevBookmark",                  CK_Prev_Bookmark },
    { "EditPageUpHighlight",               CK_Page_Up_Highlight },
    { "EditPageDownHighlight",             CK_Page_Down_Highlight },
    { "EditLeftHighlight",                 CK_Left_Highlight },
    { "EditRightHighlight",                CK_Right_Highlight },
    { "EditWordLeftHighlight",             CK_Word_Left_Highlight },
    { "EditWordRightHighlight",            CK_Word_Right_Highlight },
    { "EditUpHighlight",                   CK_Up_Highlight },
    { "EditDownHighlight",                 CK_Down_Highlight },
    { "EditHomeHighlight",                 CK_Home_Highlight },
    { "EditEndHighlight",                  CK_End_Highlight },
    { "EditBeginningOfTextHighlight",      CK_Beginning_Of_Text_Highlight },
    { "EditEndOfTextHighlight",            CK_End_Of_Text_Highlight },
    { "EditBeginPageHighlight",            CK_Begin_Page_Highlight },
    { "EditEndPageHighlight",              CK_End_Page_Highlight },
    { "EditScrollUpHighlight",             CK_Scroll_Up_Highlight },
    { "EditScrollDownHighlight",           CK_Scroll_Down_Highlight },
    { "EditParagraphUpHighlight",          CK_Paragraph_Up_Highlight },
    { "EditParagraphDownHighlight",        CK_Paragraph_Down_Highlight },
    { "EditXStore",                        CK_XStore },
    { "EditXCut",                          CK_XCut },
    { "EditXPaste",                        CK_XPaste },
    { "EditSelectionHistory",              CK_Selection_History },
    { "EditShell",                         CK_Shell },
    { "EditSelectCodepage",                CK_Select_Codepage },
    { "EditInsertLiteral",                 CK_Insert_Literal },
    { "EditExecuteMacro",                  CK_Execute_Macro },
    { "EditBeginorEndMacro",               CK_Begin_End_Macro },
    { "EditExtmode",                       CK_Ext_Mode },
    { "EditToggleLineState",               CK_Toggle_Line_State },
#if 0
    { "EditFocusNext",                     CK_Focus_Next },
    { "EditFocusPrev",                     CK_Focus_Prev },
    { "EditHeightInc",                     CK_Height_Inc },
    { "EditHeightDec",                     CK_Height_Dec },
    { "EditMake",                          CK_Make },
    { "EditErrorNext",                     CK_Error_Next },
    { "EditErrorPrev",                     CK_Error_Prev },
#endif

    /* viewer */
    { "ViewSearch",                        CK_ViewSearch },
    { "ViewContinueSearch",                CK_ViewContinueSearch },
    { "ViewGotoBookmark",                  CK_ViewGotoBookmark },
    { "ViewNewBookmark",                   CK_ViewNewBookmark },
    { "ViewMoveUp",                        CK_ViewMoveUp },
    { "ViewMoveDown",                      CK_ViewMoveDown },
    { "ViewMoveLeft",                      CK_ViewMoveLeft },
    { "ViewMoveRight",                     CK_ViewMoveRight },
    { "ViewMovePgDn",                      CK_ViewMovePgDn },
    { "ViewMovePgUp",                      CK_ViewMovePgUp },
    { "ViewMoveHalfPgDn",                  CK_ViewMoveHalfPgDn },
    { "ViewMoveHalfPgUp",                  CK_ViewMoveHalfPgUp },
    { "ViewMoveToBol",                     CK_ViewMoveToBol },
    { "ViewMoveToEol",                     CK_ViewMoveToEol },
    { "ViewNextFile",                      CK_ViewNextFile },
    { "ViewPrevFile",                      CK_ViewPrevFile },
    { "ViewToggleRuler",                   CK_ViewToggleRuler },
    { "HexViewToggleNavigationMode",       CK_HexViewToggleNavigationMode },
    { "ViewQuit",                          CK_ViewQuit },

    /* main commands */
    { "CmdChmod",                          CK_ChmodCmd },
    { "CmdChownAdvanced",                  CK_ChownAdvancedCmd },
    { "CmdChown",                          CK_ChownCmd },
    { "CmdCompareDirs",                    CK_CompareDirsCmd },
    { "CmdConfigureBox",                   CK_ConfigureBox },
    { "CmdConfigureVfs",                   CK_ConfigureVfs },
    { "CmdConfirmBox",                     CK_ConfirmBox },
    { "CmdCopy",                           CK_CopyCmd },
    { "CmdDelete",                         CK_DeleteCmd },
    { "CmdDirsizes",                       CK_DirsizesCmd },
    { "CmdDisplayBitsBox",                 CK_DisplayBitsBox },
    { "CmdEdit",                           CK_EditCmd },
    { "CmdEditMcMenu",                     CK_EditMcMenuCmd },
    { "CmdEditSymlink",                    CK_EditSymlinkCmd },
    { "CmdEditSyntax",                     CK_EditSyntaxCmd },
    { "CmdEditUserMenu",                   CK_EditUserMenuCmd },
    { "CmdExternalPanelize",               CK_ExternalPanelize },
    { "CmdFilter",                         CK_FilterCmd },
    { "CmdFilteredView",                   CK_FilteredViewCmd },
    { "CmdFind",                           CK_FindCmd },
    { "CmdFishlink",                       CK_FishlinkCmd },
    { "CmdFtplink",                        CK_FtplinkCmd },
    { "CmdHistory",                        CK_HistoryCmd },
    { "CmdInfo",                           CK_InfoCmd },
    { "CmdJobs",                           CK_JobsCmd },
    { "CmdLayout",                         CK_LayoutCmd },
    { "CmdLearnKeys",                      CK_LearnKeys },
    { "CmdLink",                           CK_LinkCmd },
    { "CmdListing",                        CK_ListingCmd },
    { "CmdMkdir",                          CK_MkdirCmd },
    { "CmdQuickCd",                        CK_QuickCdCmd },
    { "CmdQuickChdir",                     CK_QuickChdirCmd },
    { "CmdQuickView",                      CK_QuickViewCmd },
    { "CmdQuietQuit",                      CK_QuietQuitCmd },
    { "CmdRename",                         CK_RenCmd },
    { "CmdReread",                         CK_RereadCmd },
    { "CmdReselectVfs",                    CK_ReselectVfs },
    { "CmdReverseSelection",               CK_ReverseSelectionCmd },
    { "CmdSaveSetup",                      CK_SaveSetupCmd },
    { "CmdSelect",                         CK_SelectCmd },
    { "CmdSwapPanel",                      CK_SwapCmd },
    { "CmdSymlink",                        CK_SymlinkCmd },
    { "CmdTree",                           CK_TreeCmd },
    { "CmdUndelete",                       CK_UndeleteCmd },
    { "CmdUnselect",                       CK_UnselectCmd },
    { "CmdUserFileMenu",                   CK_UserFileMenuCmd },
    { "CmdView",                           CK_ViewCmd },
    { "CmdViewFile",                       CK_ViewFileCmd },
    { "CmdViewOther",                      CK_ViewOtherCmd },
    { "CmdCmdCopyCurrentReadlink",         CK_CopyCurrentReadlink },
    { "CmdCopyOtherReadlink",              CK_CopyOtherReadlink },
    { "CmdAddHotlist",                     CK_AddHotlist },
    { "CmdStartExt",                       CK_StartExtCmd },
    { "CmdQuit",                           CK_QuitCmd },
    { "CmdCopyOtherTarget",                CK_CopyOtherTarget },
    { "CmdCopyOthertReadlink",             CK_CopyOthertReadlink },

    /* panel */
    { "PanelChdirOtherPanel",              CK_PanelChdirOtherPanel },
    { "PanelChdirToReadlink",              CK_PanelChdirToReadlink },
    { "PanelCopyLocal",                    CK_PanelCmdCopyLocal },
    { "PanelDeleteLocal",                  CK_PanelCmdDeleteLocal },
    { "PanelDoEnter",                      CK_PanelCmdDoEnter },
    { "PanelEditNew",                      CK_PanelCmdEditNew },
    { "PanelRenameLocal",                  CK_PanelCmdRenameLocal },
    { "PanelReverseSelection",             CK_PanelCmdReverseSelection },
    { "PanelSelect",                       CK_PanelCmdSelect },
    { "PanelUnselect",                     CK_PanelCmdUnselect },
    { "PanelViewSimple",                   CK_PanelCmdViewSimple },
    { "PanelCtrlNextPage",                 CK_PanelCtrlNextPage },
    { "PanelCtrlPrevPage",                 CK_PanelCtrlPrevPage },
    { "PanelDirectoryHistoryList",         CK_PanelDirectoryHistoryList },
    { "PanelDirectoryHistoryNext",         CK_PanelDirectoryHistoryNext },
    { "PanelDirectoryHistoryPrev",         CK_PanelDirectoryHistoryPrev },
    { "PanelGotoBottomFile",               CK_PanelGotoBottomFile },
    { "PanelGotoMiddleFile",               CK_PanelGotoMiddleFile },
    { "PanelGotoTopFile",                  CK_PanelGotoTopFile },
    { "PanelMarkFile",                     CK_PanelMarkFile },
    { "PanelMoveUp",                       CK_PanelMoveUp },
    { "PanelMoveDown",                     CK_PanelMoveDown },
    { "PanelMoveLeft",                     CK_PanelMoveLeft },
    { "PanelMoveRight",                    CK_PanelMoveRight },
    { "PanelMoveEnd",                      CK_PanelMoveEnd },
    { "PanelMoveHome",                     CK_PanelMoveHome },
    { "PanelNextPage",                     CK_PanelNextPage },
    { "PanelPrevPage",                     CK_PanelPrevPage },
    { "PanelSetPanelEncoding",             CK_PanelSetPanelEncoding },
    { "PanelStartSearch",                  CK_PanelStartSearch },
    { "PanelSyncOtherPanel",               CK_PanelSyncOtherPanel },
    { NULL,                                0 }
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
    global_key_map_t new_bind, *map;
    guint i;
    map = &(g_array_index(keymap, global_key_map_t, 0));

    if (key !=0 && cmd !=0) {
        new_bind.key = key;
        new_bind.command = cmd;
        g_array_append_val(keymap, new_bind);
    }

}

void
keybind_cmd_bind(GArray *keymap, char *keybind, int action)
{
    keymap_add(keymap, lookup_key(keybind), action);
}
