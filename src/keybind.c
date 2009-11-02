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

static name_keymap_t command_names[] = {
#ifdef USE_INTERNAL_EDIT
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
    { "EditQuit",                          CK_Quit },
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

    { "EditShiftBlockLeft",                CK_Shift_Block_Left },
    { "EditShiftBlockRight",               CK_Shift_Block_Right },

    { "EditXStore",                        CK_XStore },
    { "EditXCut",                          CK_XCut },
    { "EditXPaste",                        CK_XPaste },
    { "EditSelectionHistory",              CK_Selection_History },
    { "EditShell",                         CK_Shell },
    { "EditInsertLiteral",                 CK_Insert_Literal },
    { "EditExecuteMacro",                  CK_Execute_Macro },
    { "EditBeginOrEndMacro",               CK_Begin_End_Macro },
    { "EditExtMode",                       CK_Ext_Mode },
    { "EditToggleLineState",               CK_Toggle_Line_State },
    { "EditToggleTabTWS",                  CK_Toggle_Tab_TWS },
    { "EditToggleSyntax",                  CK_Toggle_Syntax },
    { "EditFindDefinition",                CK_Find_Definition },
    { "EditLoadPrevFile",                  CK_Load_Prev_File },
    { "EditLoadNextFile",                  CK_Load_Next_File },
    { "EditOptions",                       CK_Edit_Options },
    { "EditSaveMode",                      CK_Edit_Save_Mode },
    { "EditChooseSyntax",                  CK_Choose_Syntax },
    { "EditAbout",                         CK_About },

#if 0
    { "EditFocusNext",                   CK_Focus_Next },
    { "EditFocusPrev",                   CK_Focus_Prev },
    { "EditHeightInc",                   CK_Height_Inc },
    { "EditHeightDec",                   CK_Height_Dec },
    { "EditMake",                        CK_Make },
    { "EditErrorNext",                   CK_Error_Next },
    { "EditErrorPrev",                   CK_Error_Prev },
#endif

#if 0
    { "EditSaveDesktop",                 CK_Save_Desktop },
    { "EditNewWindow",                   CK_New_Window },
    { "EditCycle",                       CK_Cycle },
    { "EditMenu",                        CK_Menu },
    { "EditSaveAndQuit",                 CK_Save_And_Quit },
    { "EditRunAnother",                  CK_Run_Another },
    { "EditCheckSaveAndQuit",            CK_Check_Save_And_Quit },
    { "EditMaximize",                    CK_Maximize },
#endif

#endif		/* USE_INTERNAL_EDIT */

    /* viewer */
    { "ViewSearch",                      CK_ViewSearch },
    { "ViewContinueSearch",              CK_ViewContinueSearch },
    { "ViewGotoBookmark",                CK_ViewGotoBookmark },
    { "ViewNewBookmark",                 CK_ViewNewBookmark },
    { "ViewMoveUp",                      CK_ViewMoveUp },
    { "ViewMoveDown",                    CK_ViewMoveDown },
    { "ViewMoveLeft",                    CK_ViewMoveLeft },
    { "ViewMoveRight",                   CK_ViewMoveRight },
    { "ViewMovePgDn",                    CK_ViewMovePgDn },
    { "ViewMovePgUp",                    CK_ViewMovePgUp },
    { "ViewMoveHalfPgDn",                CK_ViewMoveHalfPgDn },
    { "ViewMoveHalfPgUp",                CK_ViewMoveHalfPgUp },
    { "ViewMoveToBol",                   CK_ViewMoveToBol },
    { "ViewMoveToEol",                   CK_ViewMoveToEol },
    { "ViewNextFile",                    CK_ViewNextFile },
    { "ViewPrevFile",                    CK_ViewPrevFile },
    { "ViewToggleRuler",                 CK_ViewToggleRuler },
    { "HexViewToggleNavigationMode",     CK_HexViewToggleNavigationMode },
    { "ViewQuit",                        CK_ViewQuit },

    /* tree */
    { "TreeHelp",                        CK_TreeHelp },
    { "TreeForget",                      CK_TreeForget },
    { "TreeToggleNav",                   CK_TreeToggleNav },
    { "TreeCopy",                        CK_TreeCopy },
    { "TreeMove",                        CK_TreeMove },
    { "TreeMake",                        CK_TreeMake },
    { "TreeMoveUp",                      CK_TreeMoveUp },
    { "TreeMoveDown",                    CK_TreeMoveDown },
    { "TreeMoveLeft",                    CK_TreeMoveLeft },
    { "TreeMoveRight",                   CK_TreeMoveRight },
    { "TreeMoveHome",                    CK_TreeMoveHome },
    { "TreeMoveEnd",                     CK_TreeMoveEnd },
    { "TreeMovePgUp",                    CK_TreeMovePgUp },
    { "TreeMovePgDn",                    CK_TreeMovePgDn },
    { "TreeOpen",                        CK_TreeOpen },
    { "TreeRescan",                      CK_TreeRescan },
    { "TreeStartSearch",                 CK_TreeStartSearch },
    { "TreeRemove",                      CK_TreeRemove },

    /* main commands */
    { "CmdChmod",                        CK_ChmodCmd },
    { "CmdMenuLastSelected",             CK_MenuLastSelectedCmd },
    { "CmdSingleDirsize",                CK_SingleDirsizeCmd },
    { "CmdCopyCurrentPathname",          CK_CopyCurrentPathname },
    { "CmdCopyOtherPathname",            CK_CopyOtherPathname },
    { "CmdSuspend",                      CK_SuspendCmd },
    { "CmdToggleListing",                CK_ToggleListingCmd },
    { "CmdChownAdvanced",                CK_ChownAdvancedCmd },
    { "CmdChown",                        CK_ChownCmd },
    { "CmdCompareDirs",                  CK_CompareDirsCmd },
    { "CmdConfigureBox",                 CK_ConfigureBox },
    { "CmdConfigureVfs",                 CK_ConfigureVfs },
    { "CmdConfirmBox",                   CK_ConfirmBox },
    { "CmdCopy",                         CK_CopyCmd },
    { "CmdDelete",                       CK_DeleteCmd },
    { "CmdDirsizes",                     CK_DirsizesCmd },
    { "CmdDisplayBitsBox",               CK_DisplayBitsBox },
    { "CmdEdit",                         CK_EditCmd },
    { "CmdEditExtFile",                  CK_EditExtFileCmd },
    { "CmdEditFhlFile",                  CK_EditFhlFileCmd },
    { "CmdEditMcMenu",                   CK_EditMcMenuCmd },
    { "CmdEditSymlink",                  CK_EditSymlinkCmd },
    { "CmdEditSyntax",                   CK_EditSyntaxCmd },
    { "CmdEditUserMenu",                 CK_EditUserMenuCmd },
    { "CmdExternalPanelize",             CK_ExternalPanelize },
    { "CmdFilter",                       CK_FilterCmd },
    { "CmdFilteredView",                 CK_FilteredViewCmd },
    { "CmdFind",                         CK_FindCmd },
#ifdef USE_NETCODE
    { "CmdFishlink",                     CK_FishlinkCmd },
    { "CmdFtplink",                      CK_FtplinkCmd },
#endif
    { "CmdHistory",                      CK_HistoryCmd },
    { "CmdInfo",                         CK_InfoCmd },
#ifdef WITH_BACKGROUND
    { "CmdJobs",                         CK_JobsCmd },
#endif
    { "CmdLayout",                       CK_LayoutCmd },
    { "CmdLearnKeys",                    CK_LearnKeys },
    { "CmdLink",                         CK_LinkCmd },
    { "CmdListing",                      CK_ListingCmd },
#ifdef LISTMODE_EDITOR
    { "CmdListmodeCmd",                  CK_ListmodeCmd }.
#endif
    { "CmdMenuInfo",                     CK_MenuInfoCmd },
    { "CmdMenuQuickView",                CK_MenuQuickViewCmd },
    { "CmdMkdir",                        CK_MkdirCmd },
#if defined (USE_NETCODE) && defined (ENABLE_VFS_MCFS)
    { "CmdNetlink",                      CK_NetlinkCmd },
#endif
    { "CmdQuickCd",                      CK_QuickCdCmd },
    { "CmdQuickChdir",                   CK_QuickChdirCmd },
    { "CmdQuickView",                    CK_QuickViewCmd },
    { "CmdQuietQuit",                    CK_QuietQuitCmd },
    { "CmdRename",                       CK_RenameCmd },
    { "CmdReread",                       CK_RereadCmd },
    { "CmdReselectVfs",                  CK_ReselectVfs },
    { "CmdReverseSelection",             CK_ReverseSelectionCmd },
    { "CmdSaveSetup",                    CK_SaveSetupCmd },
    { "CmdSelect",                       CK_SelectCmd },
#if defined (USE_NETCODE) && defined (WITH_SMBFS)
    { "CmdSmblinkCmd",                   CK_SmblinkCmd },
#endif
    { "CmdSwapPanel",                    CK_SwapCmd },
    { "CmdSymlink",                      CK_SymlinkCmd },
    { "CmdTree",                         CK_TreeCmd },
    { "CmdTreeBox",                      CK_TreeBoxCmd },
#ifdef USE_EXT2FSLIB
    { "CmdUndelete",                     CK_UndeleteCmd },
#endif
    { "CmdUnselect",                     CK_UnselectCmd },
    { "CmdUserMenu",                     CK_UserMenuCmd },
    { "CmdUserFileMenu",                 CK_UserFileMenuCmd },
    { "CmdView",                         CK_ViewCmd },
    { "CmdViewFile",                     CK_ViewFileCmd },
    { "CmdCopyCurrentReadlink",          CK_CopyCurrentReadlink },
    { "CmdCopyOtherReadlink",            CK_CopyOtherReadlink },
    { "CmdAddHotlist",                   CK_AddHotlist },
    { "CmdQuit",                         CK_QuitCmd },
    { "CmdCopyCurrentTagged",            CK_CopyCurrentTagged },
    { "CmdCopyOtherTagged",              CK_CopyOtherTagged },
    { "CmdToggleShowHidden",             CK_ToggleShowHidden },

    /* panel */
    { "PanelChdirOtherPanel",            CK_PanelChdirOtherPanel },
    { "PanelChdirToReadlink",            CK_PanelChdirToReadlink },
    { "PanelCopyLocal",                  CK_PanelCmdCopyLocal },
    { "PanelDeleteLocal",                CK_PanelCmdDeleteLocal },
    { "PanelDoEnter",                    CK_PanelCmdDoEnter },
    { "PanelEditNew",                    CK_PanelCmdEditNew },
    { "PanelRenameLocal",                CK_PanelCmdRenameLocal },
    { "PanelReverseSelection",           CK_PanelCmdReverseSelection },
    { "PanelSelect",                     CK_PanelCmdSelect },
    { "PanelUnselect",                   CK_PanelCmdUnselect },
    { "PanelViewSimple",                 CK_PanelCmdViewSimple },
    { "PanelCtrlNextPage",               CK_PanelCtrlNextPage },
    { "PanelCtrlPrevPage",               CK_PanelCtrlPrevPage },
    { "PanelDirectoryHistoryList",       CK_PanelDirectoryHistoryList },
    { "PanelDirectoryHistoryNext",       CK_PanelDirectoryHistoryNext },
    { "PanelDirectoryHistoryPrev",       CK_PanelDirectoryHistoryPrev },
    { "PanelGotoBottomFile",             CK_PanelGotoBottomFile },
    { "PanelGotoMiddleFile",             CK_PanelGotoMiddleFile },
    { "PanelGotoTopFile",                CK_PanelGotoTopFile },
    { "PanelMarkFile",                   CK_PanelMarkFile },
    { "PanelMoveUp",                     CK_PanelMoveUp },
    { "PanelMoveDown",                   CK_PanelMoveDown },
    { "PanelMoveLeft",                   CK_PanelMoveLeft },
    { "PanelMoveRight",                  CK_PanelMoveRight },
    { "PanelMoveEnd",                    CK_PanelMoveEnd },
    { "PanelMoveHome",                   CK_PanelMoveHome },
    { "PanelNextPage",                   CK_PanelNextPage },
    { "PanelPrevPage",                   CK_PanelPrevPage },
#ifdef HAVE_CHARSET
    { "PanelSetPanelEncoding",           CK_PanelSetPanelEncoding },
#endif
    { "PanelStartSearch",                CK_PanelStartSearch },
    { "PanelSyncOtherPanel",             CK_PanelSyncOtherPanel },
    { "PanelToggleSortOrderNext",        CK_PanelToggleSortOrderNext },
    { "PanelToggleSortOrderPrev",        CK_PanelToggleSortOrderPrev },
    { "PanelSelectSortOrder",            CK_PanelSelectSortOrder },
    { "PanelReverseSort",                CK_PanelReverseSort },
    { "PanelSortOrderByName",            CK_PanelSortOrderByName },
    { "PanelSortOrderByExt",             CK_PanelSortOrderByExt },
    { "PanelSortOrderBySize",            CK_PanelSortOrderBySize },
    { "PanelSortOrderByMTime",           CK_PanelSortOrderByMTime },

    /* input line */
    { "InputBol",                        CK_InputBol },
    { "InputEol",                        CK_InputEol },
    { "InputMoveLeft",                   CK_InputMoveLeft },
    { "InputWordLeft",                   CK_InputWordLeft },
    { "InputBackwardChar",               CK_InputBackwardChar },
    { "InputBackwardWord",               CK_InputBackwardWord },
    { "InputMoveRight",                  CK_InputMoveRight },
    { "InputWordRight",                  CK_InputWordRight },
    { "InputForwardChar",                CK_InputForwardChar },
    { "InputForwardWord",                CK_InputForwardWord },
    { "InputBackwardDelete",             CK_InputBackwardDelete },
    { "InputDeleteChar",                 CK_InputDeleteChar },
    { "InputKillWord",                   CK_InputKillWord },
    { "InputBackwardKillWord",           CK_InputBackwardKillWord },
    { "InputSetMark",                    CK_InputSetMark },
    { "InputKillRegion",                 CK_InputKillRegion },
    { "InputYank",                       CK_InputYank },
    { "InputKillLine",                   CK_InputKillLine },
    { "InputHistoryPrev",                CK_InputHistoryPrev },
    { "InputHistoryNext",                CK_InputHistoryNext },
    { "InputHistoryShow",                CK_InputHistoryShow },
    { "InputComplete",                   CK_InputComplete },
    { "InputXStore",                     CK_InputKillSave },
    { "InputXPaste",                     CK_InputPaste },
    { "InputClearLine",                  CK_InputClearLine },

    /* common */
    { "ExtMap1",                         CK_StartExtMap1 },
    { "ExtMap2",                         CK_StartExtMap2 },
    { "ShowCommandLine",                 CK_ShowCommandLine },
    { "SelectCodepage",                  CK_SelectCodepage },

    { NULL,                              CK_Ignore_Key }
};

static const size_t num_command_names = sizeof (command_names) /
					sizeof (command_names[0]) - 1;

/*** global variables ****************************************************************************/

/* viewer/actions_cmd.c */
const global_keymap_t default_viewer_keymap[] = {
    { '?',         CK_ViewSearch,         "?" },
    { '/',         CK_ViewSearch,         "/" },
    { XCTRL ('r'), CK_ViewContinueSearch, "C-r" },
    { XCTRL ('s'), CK_ViewContinueSearch, "C-s" },
    { KEY_F (17),  CK_ViewContinueSearch, "S-F7" },
    { 'n',         CK_ViewContinueSearch, "n" },
    { ALT ('r'),   CK_ViewToggleRuler,    "M-r" },

    { XCTRL ('a'), CK_ViewMoveToBol,      "C-a" },
    { XCTRL ('e'), CK_ViewMoveToEol,      "C-e" },

    { 'h',         CK_ViewMoveLeft,       "h" },
    { KEY_LEFT,    CK_ViewMoveLeft,       "Left" },

    { 'l',         CK_ViewMoveRight,      "l" },
    { KEY_RIGHT,   CK_ViewMoveRight,      "Right" },

    { 'k',         CK_ViewMoveUp,         "k" },
    { 'y',         CK_ViewMoveUp,         "y" },
    { KEY_IC,      CK_ViewMoveUp,         "Insert" },
    { KEY_UP,      CK_ViewMoveUp,         "Up" },

    { 'j',         CK_ViewMoveDown,       "j" },
    { 'e',         CK_ViewMoveDown,       "e" },
    { KEY_DOWN,    CK_ViewMoveDown,       "Down" },
    { KEY_DC,      CK_ViewMoveDown,       "Delete" },

    { ' ',         CK_ViewMovePgDn,       "Space" },
    { 'f',         CK_ViewMovePgDn,       "f" },
    { KEY_NPAGE,   CK_ViewMovePgDn,       "PgDn" },

    { 'b',         CK_ViewMovePgUp,       "b" },
    { KEY_PPAGE,   CK_ViewMovePgUp,       "PgUp" },

    { 'd',         CK_ViewMoveHalfPgDn,   "d" },
    { 'u',         CK_ViewMoveHalfPgUp,   "u" },

    { 'm',         CK_ViewGotoBookmark,   "m" },
    { 'r',         CK_ViewNewBookmark,    "r" },

    { XCTRL ('f'), CK_ViewNextFile,       "C-f" },
    { XCTRL ('b'), CK_ViewPrevFile,       "C-b" },

    { 'q',         CK_ViewQuit,           "q" },
    { XCTRL ('g'), CK_ViewQuit,           "C-q" },
    { ESC_CHAR,    CK_ViewQuit,           "Esc" },

    { ALT ('e'),   CK_SelectCodepage,     "M-e" },
    { XCTRL ('o'), CK_ShowCommandLine,    "C-o" },

    { 0, CK_Ignore_Key, "" }
};

const global_keymap_t default_viewer_hex_keymap[] = {
    { '\t',        CK_HexViewToggleNavigationMode, "Tab" },
    { XCTRL ('a'), CK_ViewMoveToBol,               "C-a" },
    { XCTRL ('e'), CK_ViewMoveToEol,               "C-e" },

    { 'b',         CK_ViewMoveLeft,                "b" },
    { KEY_LEFT,    CK_ViewMoveLeft,                "Left" },

    { 'f',         CK_ViewMoveRight,               "f" },
    { KEY_RIGHT,   CK_ViewMoveRight,               "Right" },

    { 'k',         CK_ViewMoveUp,                  "k" },
    { 'y',         CK_ViewMoveUp,                  "y" },
    { KEY_UP,      CK_ViewMoveUp,                  "Up" },

    { 'j',         CK_ViewMoveDown,                "j" },
    { KEY_DOWN,    CK_ViewMoveDown,                "Down" },
    { KEY_DC,      CK_ViewMoveDown,                "Delete" },

    { 0, CK_Ignore_Key, "" }
};

#ifdef USE_INTERNAL_EDIT
/* ../edit/editkeys.c */
const global_keymap_t default_editor_keymap[] = {
    { '\n',                                 CK_Enter,               "Enter" },
    { '\t',                                 CK_Tab,                 "Tab" },

    { ESC_CHAR,                             CK_Quit,                "Esc" },
    { KEY_BACKSPACE,                        CK_BackSpace,           "BackSpace" },
    { KEY_DC,                               CK_Delete,              "Delete" },
    { KEY_DOWN,                             CK_Down,                "Down" },
    { KEY_END,                              CK_End,                 "End" },
    { KEY_HOME,                             CK_Home,                "Home" },
    { KEY_IC,                               CK_Toggle_Insert,       "Insert" },
    { KEY_LEFT,                             CK_Left,                "Left" },
    { KEY_NPAGE,                            CK_Page_Down,           "PgDn" },
    { KEY_PPAGE,                            CK_Page_Up,             "PgUp" },
    { KEY_RIGHT,                            CK_Right,               "Right" },
    { KEY_UP,                               CK_Up,                  "Up" },

    { ALT ('\n'),                           CK_Find_Definition,     "M-Enter" },
    { ALT ('\t'),                           CK_Complete_Word,       "M-Tab" },
    { ALT ('l'),                            CK_Goto,                "M-l" },
    { ALT ('L'),                            CK_Goto,                "M-L" },
    { ALT ('p'),                            CK_Paragraph_Format,    "M-p" },
    { ALT ('t'),                            CK_Sort,                "M-t" },
    { ALT ('u'),                            CK_ExtCmd,              "M-u" },
    { ALT ('<'),                            CK_Beginning_Of_Text,   "M-<" },
    { ALT ('>'),                            CK_End_Of_Text,         "M->" },
    { ALT ('-'),                            CK_Load_Prev_File,      "M--" },
    { ALT ('+'),                            CK_Load_Next_File,      "M-+" },
    { ALT ('d'),                            CK_Delete_Word_Right,   "M-d" },
    { ALT (KEY_BACKSPACE),                  CK_Delete_Word_Left,    "M-BackSpace" },
    { ALT ('n'),                            CK_Toggle_Line_State,   "M-n" },
    { ALT ('_'),                            CK_Toggle_Tab_TWS,      "M-_" },
    { ALT ('k'),                            CK_Toggle_Bookmark,     "M-k" },
    { ALT ('i'),                            CK_Prev_Bookmark,       "M-i" },
    { ALT ('j'),                            CK_Next_Bookmark,       "M-j" },
    { ALT ('o'),                            CK_Flush_Bookmarks,     "M-o" },

    { XCTRL ('n'),                          CK_New,                 "C-n" },
    { XCTRL ('k'),                          CK_Delete_To_Line_End,  "C-k" },
    { XCTRL ('l'),                          CK_Refresh,             "C-l" },
    { XCTRL ('o'),                          CK_Shell,               "C-o" },
    { XCTRL ('s'),                          CK_Toggle_Syntax,       "C-s" },
    { XCTRL ('u'),                          CK_Undo,                "C-u" },
    { ALT ('e'),                            CK_SelectCodepage,      "M-e" },
    { XCTRL ('q'),                          CK_Insert_Literal,      "C-q" },
    { XCTRL ('r'),                          CK_Begin_End_Macro,     "C-r" },
    { XCTRL ('r'),                          CK_Begin_Record_Macro,  "C-r" },
    { XCTRL ('r'),                          CK_End_Record_Macro,    "C-r" },
    { XCTRL ('a'),                          CK_Execute_Macro,       "C-a" },

    { KEY_F (1),                            CK_Help,                "F1" },
    { KEY_F (2),                            CK_Save,                "F2" },
    { KEY_F (3),                            CK_Mark,                "F3" },
    { KEY_F (4),                            CK_Replace,             "F4" },
    { KEY_F (5),                            CK_Copy,                "F5" },
    { KEY_F (6),                            CK_Move,                "F6" },
    { KEY_F (7),                            CK_Find,                "F7" },
    { KEY_F (8),                            CK_Remove,              "F8" },
    { KEY_F (10),                           CK_Quit,                "F10" },
    /* edit user menu */
    { KEY_F (11),                           CK_User_Menu,           "S-F1" },
    { KEY_F (12),                           CK_Save_As,             "S-F2" },
    { KEY_F (13),                           CK_Column_Mark,         "S-F3" },
    { KEY_F (14),                           CK_Replace_Again,       "S-F4" },
    { KEY_F (15),                           CK_Insert_File,         "S-F5" },
    { KEY_F (17),                           CK_Find_Again,          "S-F6" },
    /* C formatter */
    { KEY_F (19),                           CK_Pipe_Block (0),      "S-F9" },

    /* Shift */
    { KEY_M_SHIFT | KEY_NPAGE,              CK_Page_Down_Highlight, "S-PgDn" },
    { KEY_M_SHIFT | KEY_PPAGE,              CK_Page_Up_Highlight,   "S-PgUp" },
    { KEY_M_SHIFT | KEY_LEFT,               CK_Left_Highlight,      "S-Left" },
    { KEY_M_SHIFT | KEY_RIGHT,              CK_Right_Highlight,     "S-Right" },
    { KEY_M_SHIFT | KEY_UP,                 CK_Up_Highlight,        "S-Up" },
    { KEY_M_SHIFT | KEY_DOWN,               CK_Down_Highlight,      "S-Down" },
    { KEY_M_SHIFT | KEY_HOME,               CK_Home_Highlight,      "S-Home" },
    { KEY_M_SHIFT | KEY_END,                CK_End_Highlight,       "S-End" },
    { KEY_M_SHIFT | KEY_IC,                 CK_XPaste,              "S-Insert" },
    { KEY_M_SHIFT | KEY_DC,                 CK_XCut,                "S-Delete" },
    /* useful for pasting multiline text */
    { KEY_M_SHIFT | '\n',                   CK_Return,              "S-Enter" },

    /* Alt */
    { KEY_M_ALT | KEY_NPAGE,                CK_Page_Down_Alt_Highlight,     "M-PgDn" },
    { KEY_M_ALT | KEY_PPAGE,                CK_Page_Up_Alt_Highlight,       "M-PgUp" },
    { KEY_M_ALT | KEY_LEFT,                 CK_Left_Alt_Highlight,          "M-Left" },
    { KEY_M_ALT | KEY_RIGHT,                CK_Right_Alt_Highlight,         "M-Right" },
    { KEY_M_ALT | KEY_UP,                   CK_Up_Alt_Highlight,            "M-Up" },
    { KEY_M_ALT | KEY_DOWN,                 CK_Down_Alt_Highlight,          "M-Down" },
    { KEY_M_ALT | KEY_HOME,                 CK_Home_Highlight,              "M-Home" },
    { KEY_M_ALT | KEY_END,                  CK_End_Alt_Highlight,           "M-End" },

    /* Ctrl */
    { KEY_M_CTRL | (KEY_F (2)),             CK_Save_As,                     "C-F2" },
    { KEY_M_CTRL | (KEY_F (4)),             CK_Replace_Again,               "C-F4" },
    { KEY_M_CTRL | (KEY_F (7)),             CK_Find_Again,                  "C-F7" },
    { KEY_M_CTRL | KEY_BACKSPACE,           CK_Undo,                        "C-BackSpase" },
    { KEY_M_CTRL | KEY_NPAGE,               CK_End_Of_Text,                 "C-PgDn" },
    { KEY_M_CTRL | KEY_PPAGE,               CK_Beginning_Of_Text,           "C-PgUp" },
    { KEY_M_CTRL | KEY_HOME,                CK_Beginning_Of_Text,           "C-Home" },
    { KEY_M_CTRL | KEY_END,                 CK_End_Of_Text,                 "C-End" },
    { KEY_M_CTRL | KEY_UP,                  CK_Scroll_Up,                   "C-Up" },
    { KEY_M_CTRL | KEY_DOWN,                CK_Scroll_Down,                 "C-Down" },
    { KEY_M_CTRL | KEY_LEFT,                CK_Word_Left,                   "C-Left" },
    { KEY_M_CTRL | KEY_RIGHT,               CK_Word_Right,                  "C-Right" },
    { KEY_M_CTRL | KEY_IC,                  CK_XStore,                      "C-Insert" },
    { KEY_M_CTRL | KEY_DC,                  CK_Remove,                      "C-Delete" },

    /* Ctrl + Shift */
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text_Highlight,       "C-S-PgDn" },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text_Highlight, "C-S-PgUp" },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  CK_Word_Left_Highlight,         "C-S-Left" },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_Word_Right_Highlight,        "C-S-Right" },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    CK_Scroll_Up_Highlight,         "C-S-Up" },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  CK_Scroll_Down_Highlight,       "C-S-Down" },

    { XCTRL ('x'),                          CK_Ext_Mode,                    "C-x" },

    { 0, CK_Ignore_Key, "" }
};

/* emacs keyboard layout emulation */
const global_keymap_t default_editor_x_keymap[] = {
    { 'k', CK_New,           "k"},
    { 'e', CK_Execute_Macro, "e"},
    { 0, CK_Ignore_Key, "" }
};
#endif

/* tree */
const global_keymap_t default_tree_keymap[] = {
    { KEY_F (1),   CK_TreeHelp,        "F1"},
    { KEY_F (2),   CK_TreeRescan,      "F2" },
    { KEY_F (3),   CK_TreeForget,      "F3" },
    { KEY_F (4),   CK_TreeToggleNav,   "F4" },
    { KEY_F (5),   CK_TreeCopy,        "F5" },
    { KEY_F (6),   CK_TreeMove,        "F6" },
#if 0
    { KEY_F (7),   CK_TreeMake,        "F7" },
#endif
    { KEY_F (8),   CK_TreeRemove,      "F8" },
    { KEY_UP,      CK_TreeMoveUp,      "Up" },
    { XCTRL ('p'), CK_TreeMoveUp,      "C-p" },
    { KEY_DOWN,    CK_TreeMoveDown,    "Down" },
    { XCTRL ('n'), CK_TreeMoveDown,    "C-n" },
    { KEY_LEFT,    CK_TreeMoveLeft,    "Left" },
    { KEY_RIGHT,   CK_TreeMoveRight,   "Right" },
    { KEY_HOME,    CK_TreeMoveHome,    "Home" },
    { ALT ('<'),   CK_TreeMoveHome,    "M-<" },
    { KEY_END,     CK_TreeMoveEnd ,    "End" },
    { ALT ('>'),   CK_TreeMoveEnd ,    "M->" },
    { KEY_PPAGE,   CK_TreeMovePgUp,    "PgUp" },
    { ALT ('v'),   CK_TreeMovePgUp,    "M-v" },
    { KEY_NPAGE,   CK_TreeMovePgDn,    "PnDn" },
    { XCTRL ('v'), CK_TreeMovePgDn,    "C-v" },
    { '\n',        CK_TreeOpen,        "Enter" },
    { KEY_ENTER,   CK_TreeOpen,        "Enter" },
    { XCTRL ('r'), CK_TreeRescan,      "C-r" },
    { XCTRL ('s'), CK_TreeStartSearch, "C-s" },
    { ALT ('s'),   CK_TreeStartSearch, "M-s" },
    { KEY_DC,      CK_TreeRemove,      "Delete" },
    { 0, CK_Ignore_Key, ""}
};

/* screen.c */
const global_keymap_t default_panel_keymap[] = {
    { ALT ('o'),              CK_PanelChdirOtherPanel,      "M-o" },
    { ALT ('l'),              CK_PanelChdirToReadlink,      "M-l" },
    { KEY_F (15),             CK_PanelCmdCopyLocal,         "S-F5" },
    { KEY_F (18),             CK_PanelCmdDeleteLocal,       "S-F8" },
    { KEY_ENTER,              CK_PanelCmdDoEnter,           "Enter" },
    { '\n',                   CK_PanelCmdDoEnter,           "Enter" },
    { KEY_F (14),             CK_PanelCmdEditNew,           "S-F4" },
    { KEY_F (16),             CK_PanelCmdRenameLocal,       "S-F6" },
    { ALT ('*'),              CK_PanelCmdReverseSelection,  "M-*" },
    { KEY_KP_ADD,             CK_PanelCmdSelect,            "Gray+" },
    { KEY_KP_SUBTRACT,        CK_PanelCmdUnselect,          "Gray-" },
    { KEY_F (13),             CK_PanelCmdViewSimple,        "S-F3" },
    { KEY_M_CTRL | KEY_NPAGE, CK_PanelCtrlNextPage,         "C-PgDn" },
    { KEY_M_CTRL | KEY_PPAGE, CK_PanelCtrlPrevPage,         "C-PgUp" },
    { ALT ('H'),              CK_PanelDirectoryHistoryList, "M-H" },
    { ALT ('u'),              CK_PanelDirectoryHistoryNext, "M-u" },
    { ALT ('y'),              CK_PanelDirectoryHistoryPrev, "M-y" },
    { ALT ('j'),              CK_PanelGotoBottomFile,       "M-j" },
    { ALT ('r'),              CK_PanelGotoMiddleFile,       "M-r" },
    { ALT ('g'),              CK_PanelGotoTopFile,          "M-g" },
    { KEY_IC,                 CK_PanelMarkFile,             "Insert" },
    { KEY_UP,                 CK_PanelMoveUp,               "Up" },
    { KEY_DOWN,               CK_PanelMoveDown,             "Down" },
    { KEY_LEFT,               CK_PanelMoveLeft,             "Left" },
    { KEY_RIGHT,              CK_PanelMoveRight,            "Right" },
    { KEY_END,                CK_PanelMoveEnd,              "End" },
    { KEY_HOME,               CK_PanelMoveHome,             "Home" },
    { KEY_A1,                 CK_PanelMoveHome,             "Home" },
    { KEY_NPAGE,              CK_PanelNextPage,             "PgDn" },
    { KEY_PPAGE,              CK_PanelPrevPage,             "PgUp" },
    { ALT ('e'),              CK_PanelSetPanelEncoding,     "M-e" },
    { XCTRL ('s'),            CK_PanelStartSearch,          "C-s" },
    { ALT ('s'),              CK_PanelStartSearch,          "M-s" },
    { ALT ('i'),              CK_PanelSyncOtherPanel,       "M-i" },
    { 0, CK_Ignore_Key , "" }
};

/* main.c */
const global_keymap_t default_main_map[] = {
    { KEY_F (2),    CK_UserMenuCmd,                   "F2" },
    { KEY_F (3),    CK_ViewCmd,                       "F3" },
    { KEY_F (4),    CK_EditCmd,                       "F4" },
    { KEY_F (5),    CK_CopyCmd,                       "F5" },
    { KEY_F (6),    CK_RenameCmd,                     "F6" },
    { KEY_F (7),    CK_MkdirCmd,                      "F7" },
    { KEY_F (8),    CK_DeleteCmd,                     "F6" },
    { KEY_F (10),   CK_QuitCmd,                       "F10" },
    { KEY_F (13),   CK_ViewFileCmd,                   "S-F3" },
    { KEY_F (19),   CK_MenuLastSelectedCmd,           "S-F9" },
    { KEY_F (20),   CK_QuietQuitCmd,                  "S-10" },
    { ALT ('h'),    CK_HistoryCmd,                    "M-h" },
    { XCTRL ('@'),  CK_SingleDirsizeCmd,              "C-Space" },
    /* Copy useful information to the command line */
    { ALT ('a'),    CK_CopyCurrentPathname,           "M-a" },
    { ALT ('A'),    CK_CopyOtherPathname,             "M-A" },
    { ALT ('c'),    CK_QuickCdCmd,                    "M-c" },
    /* To access the directory hotlist */
    { XCTRL ('\\'), CK_QuickChdirCmd,                 "C-\\" },
    /* Suspend */
    { XCTRL ('z'),  CK_SuspendCmd,                    "C-z" },
    /* The filtered view command */
    { ALT ('!'),    CK_FilteredViewCmd,               "M-!" },
    /* Find file */
    { ALT ('?'),    CK_FindCmd,                       "M-?" },
    /* Panel refresh */
    { XCTRL ('r'),  CK_RereadCmd,                     "C-r" },
    /* Toggle listing between long, user defined and full formats */
    { ALT ('t'),    CK_ToggleListingCmd,              "M-t" },
    /* Swap panels */
    { XCTRL ('u'),  CK_SwapCmd,                       "C-u" },
    /* View output */
    { XCTRL ('o'),  CK_ShowCommandLine,               "C-o" },
    { ALT ('.'),    CK_ToggleShowHidden,              "M-." },
    { XCTRL ('x'),  CK_StartExtMap1,                  "C-x" },
    /* Select/unselect group */
    { ALT ('*'),              CK_SelectCmd,           "M-*" },
    { KEY_KP_ADD,             CK_UnselectCmd,         "Gray+" },
    { KEY_KP_SUBTRACT,        CK_ReverseSelectionCmd, "Gray-" },
    { 0, CK_Ignore_Key, "" }
};

const global_keymap_t default_main_x_map[] = {
    { XCTRL ('c'), CK_QuitCmd,             "C-c" },
    { 'd',         CK_CompareDirsCmd,      "d" },
#ifdef USE_VFS
    { 'a',         CK_ReselectVfs,         "a"},
#endif				/* USE_VFS */
    { 'p',         CK_CopyCurrentPathname, "p" },
    { XCTRL ('p'), CK_CopyOtherPathname,   "C-p" },
    { 't',         CK_CopyCurrentTagged,   "t" },
    { XCTRL ('t'), CK_CopyOtherTagged,     "C-t" },
    { 'c',         CK_ChmodCmd,            "c" },
    { 'o',         CK_ChownCmd,            "o" },
    { 'r',         CK_CopyCurrentReadlink, "r" },
    { XCTRL ('r'), CK_CopyOtherReadlink,   "C-r" },
    { 'l',         CK_LinkCmd,             "l" },
    { 's',         CK_SymlinkCmd,          "s" },
    { XCTRL ('s'), CK_EditSymlinkCmd,      "C-s" },
    { 'i',         CK_MenuInfoCmd,         "i" },
    { 'i',         CK_InfoCmd,             "i" },
    { 'q',         CK_QuickViewCmd,        "q" },
    { 'h',         CK_AddHotlist,          "h" },
    { '!',         CK_ExternalPanelize,    "!" },
#ifdef WITH_BACKGROUND
    { 'j',         CK_JobsCmd,             "j" },
#endif				/* WITH_BACKGROUND */
    { 0, CK_Ignore_Key, "" }
};

const global_keymap_t default_input_keymap[] = {
    /* Motion */
    { XCTRL ('a'),            CK_InputBol,              "C-a" },
    { KEY_HOME,               CK_InputBol,              "Home" },
    { KEY_A1,                 CK_InputBol,              "Home" },
    { ALT ('<'),              CK_InputBol,              "M-<" },
    { XCTRL ('e'),            CK_InputEol,              "C-e" },
    { KEY_END,                CK_InputEol,              "End" },
    { KEY_C1,                 CK_InputEol,              "End" },
    { ALT ('>'),              CK_InputEol,              "M->" },
    { KEY_LEFT,               CK_InputMoveLeft,         "left" },
    { KEY_M_CTRL | KEY_LEFT,  CK_InputWordLeft,         "C-Left" },
    { KEY_RIGHT,              CK_InputMoveRight,        "Right" },
    { KEY_M_CTRL | KEY_RIGHT, CK_InputWordRight,        "C-Right" },

    { XCTRL ('b'),            CK_InputBackwardChar,     "C-b" },
    { ALT ('b'),              CK_InputBackwardWord,     "M-b" },
    { XCTRL ('f'),            CK_InputForwardChar,      "C-f" },
    { ALT ('f'),              CK_InputForwardWord,      "M-f" },

    /* Editing */
    { KEY_BACKSPACE,          CK_InputBackwardDelete,   "BackSpace" },
    { KEY_DC,                 CK_InputDeleteChar,       "Delete" },
    { ALT ('d'),              CK_InputKillWord,         "M-d" },
    { ALT (KEY_BACKSPACE),    CK_InputBackwardKillWord, "M-BackSpace" },

    /* Region manipulation */
    { XCTRL ('w'),            CK_InputKillRegion,       "C-w" },
    { ALT ('w'),              CK_InputKillSave,         "M-w" },
    { XCTRL ('y'),            CK_InputYank,             "C-y" },
    { XCTRL ('k'),            CK_InputKillLine,         "C-k" },

    /* History */
    { ALT ('p'),              CK_InputHistoryPrev,      "M-p" },
    { ALT ('n'),              CK_InputHistoryNext,      "M-n" },
    { ALT ('h'),              CK_InputHistoryShow,      "M-h" },

    /* Completion */
    { ALT ('\t'),             CK_InputComplete,         "M-tab" },

    { 0, CK_Ignore_Key, "" }
};

static int
name_keymap_comparator (const void *p1, const void *p2)
{
    const name_keymap_t *m1 = (const name_keymap_t *) p1;
    const name_keymap_t *m2 = (const name_keymap_t *) p2;

    return str_casecmp (m1->name, m2->name);
}

static void
sort_command_names (void)
{
    static gboolean has_been_sorted = FALSE;

    if (!has_been_sorted) {
	qsort (command_names, num_command_names,
		sizeof (command_names[0]), &name_keymap_comparator);
	has_been_sorted = TRUE;
    }
}

int
lookup_action (const char *keyname)
{
    const name_keymap_t key = { keyname, 0 };
    name_keymap_t *res;

    sort_command_names ();

    res = bsearch (&key, command_names, num_command_names,
		    sizeof (command_names[0]), name_keymap_comparator);

    return (res != NULL) ? res->val : CK_Ignore_Key;
}

static void
keymap_add (GArray *keymap, int key, int cmd, const char *caption)
{
    if (key != 0 && cmd != 0) {
        global_keymap_t new_bind;

        new_bind.key = key;
        new_bind.command = cmd;
        g_snprintf (new_bind.caption, sizeof (new_bind.caption), "%s", caption);
        g_array_append_val (keymap, new_bind);
    }
}

void
keybind_cmd_bind (GArray *keymap, const char *keybind, int action)
{
    char *caption = NULL;
    int key;

    key = lookup_key (keybind, &caption);
    keymap_add (keymap, key, action, caption);
    g_free (caption);
}

const char *
lookup_keymap_shortcut (const global_keymap_t *keymap, int action)
{
    unsigned int i;

    for (i = 0; keymap[i].key != 0; i++)
	if (keymap[i].command == action)
	    return (keymap[i].caption[0] != '\0') ? keymap[i].caption : NULL;

    return NULL;
}
