/*
    Copyright (C) 2009 The Free Software Foundation, Inc.

    Written by:    2005        Vitja Makarov
                   2009        Ilia Maslakov

    This file is part of the Midnight Commander.

    The Midnight Commander is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    The Midnight Commander is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301, USA.
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
#include "strutil.h"

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

    { "EditPageUpAltHighlight",            CK_Page_Up_Alt_Highlight },
    { "EditPageDownAltHighlight",          CK_Page_Down_Alt_Highlight },
    { "EditLeftAltHighlight",              CK_Left_Alt_Highlight },
    { "EditRightAltHighlight",             CK_Right_Alt_Highlight },
    { "EditWordLeftAltHighlight",          CK_Word_Left_Alt_Highlight },
    { "EditWordRightAltHighlight",         CK_Word_Right_Alt_Highlight },
    { "EditUpAltHighlight",                CK_Up_Alt_Highlight },
    { "EditDownAltHighlight",              CK_Down_Alt_Highlight },
    { "EditHomeAltHighlight",              CK_Home_Alt_Highlight },
    { "EditEndAltHighlight",               CK_End_Alt_Highlight },
    { "EditBeginningOfTextAltHighlight",   CK_Beginning_Of_Text_Alt_Highlight },
    { "EditEndOfTextAltHighlight",         CK_End_Of_Text_Alt_Highlight },
    { "EditBeginPageAltHighlight",         CK_Begin_Page_Alt_Highlight },
    { "EditEndPageAltHighlight",           CK_End_Page_Alt_Highlight },
    { "EditScrollUpAltHighlight",          CK_Scroll_Up_Alt_Highlight },
    { "EditScrollDownAltHighlight",        CK_Scroll_Down_Alt_Highlight },
    { "EditParagraphUpAltHighlight",       CK_Paragraph_Up_Alt_Highlight },
    { "EditParagraphDownAltHighlight",     CK_Paragraph_Down_Alt_Highlight },

    { "EditXStore",                        CK_XStore },
    { "EditXCut",                          CK_XCut },
    { "EditXPaste",                        CK_XPaste },
    { "EditSelectionHistory",              CK_Selection_History },
    { "EditShell",                         CK_Shell },
    { "EditInsertLiteral",                 CK_Insert_Literal },
    { "EditExecuteMacro",                  CK_Execute_Macro },
    { "EditBeginorEndMacro",               CK_Begin_End_Macro },
    { "EditExtmode",                       CK_Ext_Mode },
    { "EditToggleLineState",               CK_Toggle_Line_State },
    { "EditToggleTabTWS",                  CK_Toggle_Tab_TWS },
    { "EditFindDefinition",                CK_Find_Definition },
    { "EditLoadPrevFile",                  CK_Load_Prev_File },
    { "EditLoadNextFile",                  CK_Load_Next_File },

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
    { "CmdMenuLastSelected",               CK_MenuLastSelectedCmd },
    { "CmdSingleDirsize",                  CK_SingleDirsizeCmd },
    { "CmdCopyCurrentPathname",            CK_CopyCurrentPathname },
    { "CmdCopyOtherPathname",              CK_CopyOtherPathname },
    { "CmdSuspend",                        CK_SuspendCmd },
    { "CmdToggleListing",                  CK_ToggleListingCmd },
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
    { "CmdCopyCurrentReadlink",            CK_CopyCurrentReadlink },
    { "CmdCopyOtherReadlink",              CK_CopyOtherReadlink },
    { "CmdAddHotlist",                     CK_AddHotlist },
    { "CmdQuit",                           CK_QuitCmd },
    { "CmdCopyOtherTarget",                CK_CopyOtherTarget },
    { "CmdToggleShowHidden",               CK_ToggleShowHidden },

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

    /* widgets */
    { "InputBol",                         CK_InputBol },
    { "InputEol",                         CK_InputEol },
    { "InputMoveLeft",                    CK_InputMoveLeft },
    { "InputWordLeft",                    CK_InputWordLeft },
    { "InputBackwardChar",                CK_InputBackwardChar },
    { "InputBackwardWord",                CK_InputBackwardWord },
    { "InputMoveRight",                   CK_InputMoveRight },
    { "InputWordRight",                   CK_InputWordRight },
    { "InputForwardChar",                 CK_InputForwardChar },
    { "InputForwardWord",                 CK_InputForwardWord },
    { "InputBackwardDelete",              CK_InputBackwardDelete },
    { "InputDeleteChar",                  CK_InputDeleteChar },
    { "InputKillWord",                    CK_InputKillWord },
    { "InputBackwardKillWord",            CK_InputBackwardKillWord },
    { "InputSetMark",                     CK_InputSetMark },
    { "InputKillRegion",                  CK_InputKillRegion },
    { "InputKillSave",                    CK_InputKillSave },
    { "InputYank",                        CK_InputYank },
    { "InputKillLine",                    CK_InputKillLine },
    { "InputHistoryPrev",                 CK_InputHistoryPrev },
    { "InputHistoryNext",                 CK_InputHistoryNext },
    { "InputHistoryShow",                 CK_InputHistoryShow },
    { "InputComplete",                    CK_InputComplete },

    /* common */
    { "ExtMap1",                           CK_StartExtMap1 },
    { "ExtMap2",                           CK_StartExtMap2 },
    { "ShowCommandLine",                   CK_ShowCommandLine },
    { "SelectCodepage",                    CK_SelectCodepage },

    { NULL,                                0 }
};

/*** global variables ****************************************************************************/

/* viewer/actions_cmd.c */
const global_key_map_t default_viewer_keymap[] = {

    { '?',                                  CK_ViewSearch },
    { '/',                                  CK_ViewSearch },
    { XCTRL('r'),                           CK_ViewContinueSearch },
    { XCTRL('s'),                           CK_ViewContinueSearch },
    { KEY_F (17),                           CK_ViewContinueSearch },
    { ALT('r'),                             CK_ViewToggleRuler  },

    { KEY_HOME,                             CK_ViewMoveToBol },
    { KEY_END,                              CK_ViewMoveToEol },

    { 'h',                                  CK_ViewMoveLeft },
    { KEY_LEFT,                             CK_ViewMoveLeft },

    { 'l',                                  CK_ViewMoveRight },
    { KEY_RIGHT,                            CK_ViewMoveRight },

    { 'k',                                  CK_ViewMoveUp },
    { 'y',                                  CK_ViewMoveUp },
    { KEY_IC,                               CK_ViewMoveUp },
    { KEY_UP,                               CK_ViewMoveUp },

    { 'j',                                  CK_ViewMoveDown },
    { 'e',                                  CK_ViewMoveDown },
    { KEY_DOWN,                             CK_ViewMoveDown },
    { KEY_DC,                               CK_ViewMoveDown },

    { ' ',                                  CK_ViewMovePgDn },
    { 'f',                                  CK_ViewMovePgDn },
    { KEY_NPAGE,                            CK_ViewMovePgDn },

    { 'b',                                  CK_ViewMovePgUp },
    { KEY_PPAGE,                            CK_ViewMovePgUp },

    { 'd',                                  CK_ViewMoveHalfPgDn },
    { 'u',                                  CK_ViewMoveHalfPgUp },

    { 'm',                                  CK_ViewGotoBookmark },
    { 'r',                                  CK_ViewNewBookmark },

    { XCTRL ('f'),                          CK_ViewNextFile },
    { XCTRL ('b'),                          CK_ViewPrevFile },

    { 'q',                                  CK_ViewQuit },
    { XCTRL ('g'),                          CK_ViewQuit },
    { ESC_CHAR,                             CK_ViewQuit },

    { XCTRL ('t'),                          CK_SelectCodepage },
    { XCTRL('o'),                           CK_ShowCommandLine },
    { 0, 0 }
};

const global_key_map_t default_viewer_hex_keymap[] = {

    { '\t',                                 CK_HexViewToggleNavigationMode },
    { XCTRL ('a'),                          CK_ViewMoveToBol },
    { XCTRL ('e'),                          CK_ViewMoveToEol },

    { 'b',                                  CK_ViewMoveLeft },
    { KEY_LEFT,                             CK_ViewMoveLeft },

    { 'f',                                  CK_ViewMoveRight },
    { KEY_RIGHT,                            CK_ViewMoveRight },

    { 'k',                                  CK_ViewMoveUp },
    { 'y',                                  CK_ViewMoveUp },
    { KEY_UP,                               CK_ViewMoveUp },

    { 'j',                                  CK_ViewMoveDown },
    { KEY_DOWN,                             CK_ViewMoveDown },
    { KEY_DC,                               CK_ViewMoveDown },

    { 0, 0 }
};

/* ../edit/editkeys.c */
const global_key_map_t default_editor_keymap[] = {
    { '\n',                                 CK_Enter },
    { '\t',                                 CK_Tab },

    { ESC_CHAR,                             CK_Exit },
    { KEY_BACKSPACE,                        CK_BackSpace },
    { KEY_DC,                               CK_Delete },
    { KEY_DOWN,                             CK_Down },
    { KEY_END,                              CK_End },
    { KEY_HOME,                             CK_Home },
    { KEY_IC,                               CK_Toggle_Insert },
    { KEY_LEFT,                             CK_Left },
    { KEY_NPAGE,                            CK_Page_Down },
    { KEY_PPAGE,                            CK_Page_Up },
    { KEY_RIGHT,                            CK_Right },
    { KEY_UP,                               CK_Up },

    { ALT ('\n'),                           CK_Find_Definition },
    { ALT ('\t'),                           CK_Complete_Word },
    { ALT ('l'),                            CK_Goto },
    { ALT ('L'),                            CK_Goto },
    { ALT ('p'),                            CK_Paragraph_Format },
    { ALT ('t'),                            CK_Sort },
    { ALT ('u'),                            CK_ExtCmd },
    { ALT ('<'),                            CK_Beginning_Of_Text },
    { ALT ('>'),                            CK_End_Of_Text },
    { ALT ('-'),                            CK_Load_Prev_File },
    { ALT ('='),                            CK_Load_Next_File },
    { ALT ('d'),                            CK_Delete_Word_Right },
    { ALT (KEY_BACKSPACE),                  CK_Delete_Word_Left },
    { ALT ('n'),                            CK_Toggle_Line_State },
    { ALT ('_'),                            CK_Toggle_Tab_TWS },
    { ALT ('k'),                            CK_Toggle_Bookmark },
    { ALT ('i'),                            CK_Prev_Bookmark },
    { ALT ('j'),                            CK_Next_Bookmark },
    { ALT ('o'),                            CK_Flush_Bookmarks },

    { XCTRL ('k'),                          CK_Delete_To_Line_End },
    { XCTRL ('l'),                          CK_Refresh },
    { XCTRL ('o'),                          CK_Shell },
    { XCTRL ('s'),                          CK_Toggle_Syntax },
    { XCTRL ('u'),                          CK_Undo },
    { XCTRL ('t'),                          CK_SelectCodepage },
    { XCTRL ('q'),                          CK_Insert_Literal },
    { XCTRL ('a'),                          CK_Execute_Macro },
    { XCTRL ('r'),                          CK_Begin_End_Macro },

    { KEY_F (1),                            CK_Help },
    { KEY_F (2),                            CK_Save },
    { KEY_F (3),                            CK_Mark },
    { KEY_F (4),                            CK_Replace },
    { KEY_F (5),                            CK_Copy },
    { KEY_F (6),                            CK_Move },
    { KEY_F (7),                            CK_Find },
    { KEY_F (8),                            CK_Remove },
    { KEY_F (10),                           CK_Exit },
    /* edit user menu */
    { KEY_F (11),                           CK_User_Menu },
    { KEY_F (12),                           CK_Save_As },
    { KEY_F (13),                           CK_Column_Mark },
    { KEY_F (14),                           CK_Replace_Again },
    { KEY_F (15),                           CK_Insert_File },
    { KEY_F (17),                           CK_Find_Again },
    /* C formatter */
    { KEY_F (19),                           CK_Pipe_Block (0) },

    /* Shift */
    { KEY_M_SHIFT | KEY_PPAGE,              CK_Page_Up_Highlight },
    { KEY_M_SHIFT | KEY_NPAGE,              CK_Page_Down_Highlight },
    { KEY_M_SHIFT | KEY_LEFT,               CK_Left_Highlight },
    { KEY_M_SHIFT | KEY_RIGHT,              CK_Right_Highlight },
    { KEY_M_SHIFT | KEY_UP,                 CK_Up_Highlight },
    { KEY_M_SHIFT | KEY_DOWN,               CK_Down_Highlight },
    { KEY_M_SHIFT | KEY_HOME,               CK_Home_Highlight },
    { KEY_M_SHIFT | KEY_END,                CK_End_Highlight },
    { KEY_M_SHIFT | KEY_IC,                 CK_XPaste },
    { KEY_M_SHIFT | KEY_DC,                 CK_XCut },
    /* useful for pasting multiline text */
    { KEY_M_SHIFT | '\n',                   CK_Return },

    /* Alt */
    { KEY_M_ALT | KEY_PPAGE,                CK_Page_Up_Alt_Highlight },
    { KEY_M_ALT | KEY_NPAGE,                CK_Page_Down_Alt_Highlight },
    { KEY_M_ALT | KEY_LEFT,                 CK_Left_Alt_Highlight },
    { KEY_M_ALT | KEY_RIGHT,                CK_Right_Alt_Highlight },
    { KEY_M_ALT | KEY_UP,                   CK_Up_Alt_Highlight },
    { KEY_M_ALT | KEY_DOWN,                 CK_Down_Alt_Highlight },
    { KEY_M_ALT | KEY_HOME,                 CK_Home_Highlight },
    { KEY_M_ALT | KEY_END,                  CK_End_Alt_Highlight },

    /* Ctrl */
    { KEY_M_CTRL | (KEY_F (2)),             CK_Save_As },
    { KEY_M_CTRL | (KEY_F (4)),             CK_Replace_Again },
    { KEY_M_CTRL | (KEY_F (7)),             CK_Find_Again },
    { KEY_M_CTRL | KEY_BACKSPACE,           CK_Undo },
    { KEY_M_CTRL | KEY_PPAGE,               CK_Beginning_Of_Text },
    { KEY_M_CTRL | KEY_NPAGE,               CK_End_Of_Text },
    { KEY_M_CTRL | KEY_HOME,                CK_Beginning_Of_Text },
    { KEY_M_CTRL | KEY_END,                 CK_End_Of_Text },
    { KEY_M_CTRL | KEY_UP,                  CK_Scroll_Up },
    { KEY_M_CTRL | KEY_DOWN,                CK_Scroll_Down },
    { KEY_M_CTRL | KEY_LEFT,                CK_Word_Left },
    { KEY_M_CTRL | KEY_RIGHT,               CK_Word_Right },
    { KEY_M_CTRL | KEY_IC,                  CK_XStore },
    { KEY_M_CTRL | KEY_DC,                  CK_Remove },

    /* Ctrl + Shift */
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text_Highlight },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text_Highlight },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  CK_Word_Left_Highlight },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_Word_Right_Highlight },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    CK_Scroll_Up_Highlight },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  CK_Scroll_Down_Highlight },

    { XCTRL ('x'),                          CK_StartExtMap1 },

    { 0, 0 }
};

/* screen.c */
const global_key_map_t default_panel_keymap[] = {

    { ALT('o'),                              CK_PanelChdirOtherPanel },
    { ALT('l'),                              CK_PanelChdirToReadlink },
    { KEY_F(15),                             CK_PanelCmdCopyLocal },
    { KEY_F(18),                             CK_PanelCmdDeleteLocal },
    { KEY_ENTER,                             CK_PanelCmdDoEnter },
    { '\n',                                  CK_PanelCmdDoEnter },
    { KEY_F(14),                             CK_PanelCmdEditNew },
    { KEY_F(16),                             CK_PanelCmdRenameLocal },
    { ALT('*'),                              CK_PanelCmdReverseSelection },
    { KEY_KP_ADD,                            CK_PanelCmdSelect },
    { KEY_KP_SUBTRACT,                       CK_PanelCmdUnselect },
    { KEY_F(13),                             CK_PanelCmdViewSimple },
    { KEY_M_CTRL | KEY_NPAGE,                CK_PanelCtrlNextPage },
    { KEY_M_CTRL | KEY_PPAGE,                CK_PanelCtrlPrevPage },
    { ALT('H'),                              CK_PanelDirectoryHistoryList },
    { ALT('u'),                              CK_PanelDirectoryHistoryNext },
    { ALT('y'),                              CK_PanelDirectoryHistoryPrev },
    { ALT('j'),                              CK_PanelGotoBottomFile },
    { ALT('r'),                              CK_PanelGotoMiddleFile },
    { ALT('g'),                              CK_PanelGotoTopFile },
    { KEY_IC,                                CK_PanelMarkFile },
    { KEY_UP,                                CK_PanelMoveUp },
    { KEY_DOWN,                              CK_PanelMoveDown },
    { KEY_LEFT,                              CK_PanelMoveLeft },
    { KEY_RIGHT,                             CK_PanelMoveRight },
    { KEY_END,                               CK_PanelMoveEnd },
    { KEY_HOME,                              CK_PanelMoveHome },
    { KEY_A1,                                CK_PanelMoveHome },
    { KEY_NPAGE,                             CK_PanelNextPage },
    { KEY_PPAGE,                             CK_PanelPrevPage },
    { XCTRL('t'),                            CK_PanelSetPanelEncoding },
    { XCTRL('s'),                            CK_PanelStartSearch },
    { ALT('i'),                              CK_PanelSyncOtherPanel },
    { 0, 0 }
};

/* main.c */
const global_key_map_t default_main_map[] = {
    {KEY_F (19), CK_MenuLastSelectedCmd},
    {KEY_F (20), CK_QuietQuitCmd},
    {XCTRL ('@'), CK_SingleDirsizeCmd},
    /* Copy useful information to the command line */
    {ALT ('a'), CK_CopyCurrentPathname},
    {ALT ('A'), CK_CopyOtherPathname},
    {ALT ('c'), CK_QuickCdCmd},
    /* To access the directory hotlist */
    {XCTRL ('\\'), CK_QuickChdirCmd},
    /* Suspend */
    {XCTRL ('z'), CK_SuspendCmd},
    /* The filtered view command */
    {ALT ('!'), CK_FilteredViewCmd},
    /* Find file */
    {ALT ('?'), CK_FindCmd},
    /* Panel refresh */
    {XCTRL ('r'), CK_RereadCmd},
    /* Toggle listing between long, user defined and full formats */
    {ALT ('t'), CK_ToggleListingCmd},
    /* Swap panels */
    {XCTRL ('u'), CK_SwapCmd},
    /* View output */
    {XCTRL ('o'), CK_ShowCommandLine},
    {ALT ('.'), CK_ToggleShowHidden},
    {XCTRL ('x'), CK_StartExtMap1 },
    { 0, 0 }
};

const global_key_map_t default_main_x_map[] = {
    {XCTRL ('c'), CK_QuitCmd},
    {'d', CK_CompareDirsCmd},
#ifdef USE_VFS
    {'a', CK_ReselectVfs},
#endif				/* USE_VFS */
    {'p', CK_CopyCurrentPathname},
    {XCTRL ('p'), CK_CopyOtherPathname},
    {'t', CK_CopyCurrentTagged},
    {XCTRL ('t'), CK_CopyOtherTarget},
    {'c', CK_ChmodCmd},
    {'o', CK_ChownCmd},
    {'r', CK_CopyCurrentReadlink},
    {XCTRL ('r'), CK_CopyOtherReadlink},
    {'l', CK_LinkCmd},
    {'s', CK_SymlinkCmd},
    {XCTRL ('s'), CK_EditSymlinkCmd},
    {'i', CK_InfoCmd},
    {'q', CK_QuickViewCmd},
    {'h', CK_AddHotlist},
    {'!', CK_ExternalPanelize},
#ifdef WITH_BACKGROUND
    {'j', CK_JobsCmd},
#endif				/* WITH_BACKGROUND */
    {0, 0}
};

const global_key_map_t default_input_keymap[] = {
    /* Motion */
    { XCTRL('a'),               CK_InputBol },
    { KEY_HOME,                 CK_InputBol },
    { KEY_A1,                   CK_InputBol },
    { ALT ('<'),                CK_InputBol },
    { XCTRL('e'),               CK_InputEol },
    { KEY_END,                  CK_InputEol },
    { KEY_C1,                   CK_InputEol },
    { ALT ('>'),                CK_InputEol },
    { KEY_LEFT,                 CK_InputMoveLeft },
    { KEY_LEFT | KEY_M_CTRL,    CK_InputWordLeft },
    { KEY_RIGHT,                CK_InputMoveRight },
    { KEY_RIGHT | KEY_M_CTRL,   CK_InputWordRight },

    { XCTRL('b'),               CK_InputBackwardChar },
    { ALT('b'),                 CK_InputBackwardWord },
    { XCTRL('f'),               CK_InputForwardChar },
    { ALT('f'),                 CK_InputForwardWord },

    /* Editing */
    { KEY_BACKSPACE,            CK_InputBackwardDelete },
    { KEY_DC,                   CK_InputDeleteChar },
    { ALT('d'),                 CK_InputKillWord },
    { ALT(KEY_BACKSPACE),       CK_InputBackwardKillWord },

    /* Region manipulation */
    { XCTRL('w'),               CK_InputKillRegion },
    { ALT('w'),                 CK_InputKillSave },
    { XCTRL('y'),               CK_InputYank },
    { XCTRL('k'),               CK_InputKillLine },

    /* History */
    { ALT('p'),                 CK_InputHistoryPrev },
    { ALT('n'),                 CK_InputHistoryNext },
    { ALT('h'),                 CK_InputHistoryShow },

    /* Completion */
    { ALT('\t'),                CK_InputComplete },
    { 0, 0 }
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
