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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/tty/key.h"        /* KEY_M_ */
#include "lib/strutil.h"        /* str_casecmp() */
#include "lib/keybind.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static name_keymap_t command_names[] = {
#ifdef USE_INTERNAL_EDIT
    {"EditNoCommand", CK_Ignore_Key},
    {"EditIgnoreKey", CK_Ignore_Key},
    {"EditBackSpace", CK_BackSpace},
    {"EditDelete", CK_Delete},
    {"EditEnter", CK_Enter},
    {"EditPageUp", CK_Page_Up},
    {"EditPageDown", CK_Page_Down},
    {"EditLeft", CK_Left},
    {"EditRight", CK_Right},
    {"EditWordLeft", CK_Word_Left},
    {"EditWordRight", CK_Word_Right},
    {"EditUp", CK_Up},
    {"EditDown", CK_Down},
    {"EditHome", CK_Home},
    {"EditEnd", CK_End},
    {"EditTab", CK_Tab},
    {"EditUndo", CK_Undo},
    {"EditBeginningOfText", CK_Beginning_Of_Text},
    {"EditEndOfText", CK_End_Of_Text},
    {"EditScrollUp", CK_Scroll_Up},
    {"EditScrollDown", CK_Scroll_Down},
    {"EditReturn", CK_Return},
    {"EditBeginPage", CK_Begin_Page},
    {"EditEndPage", CK_End_Page},
    {"EditDeleteWordLeft", CK_Delete_Word_Left},
    {"EditDeleteWordRight", CK_Delete_Word_Right},
    {"EditParagraphUp", CK_Paragraph_Up},
    {"EditParagraphDown", CK_Paragraph_Down},
    {"EditMenu", CK_Menu},
    {"EditSave", CK_Save},
    {"EditLoad", CK_Load},
    {"EditNew", CK_New},
    {"EditSaveas", CK_Save_As},
    {"EditMark", CK_Mark},
    {"EditCopy", CK_Copy},
    {"EditMove", CK_Move},
    {"EditRemove", CK_Remove},
    {"EditMarkAll", CK_Mark_All},
    {"EditUnmark", CK_Unmark},
    {"EditMarkWord", CK_Mark_Word},
    {"EditMarkLine", CK_Mark_Line},
    {"EditSaveBlock", CK_Save_Block},
    {"EditColumnMark", CK_Column_Mark},
    {"EditFind", CK_Find},
    {"EditFindAgain", CK_Find_Again},
    {"EditReplace", CK_Replace},
    {"EditReplaceAgain", CK_Replace_Again},
    {"EditCompleteWord", CK_Complete_Word},

#if 0
    {"EditDebugStart", CK_Debug_Start},
    {"EditDebugStop", CK_Debug_Stop},
    {"EditDebugToggleBreak", CK_Debug_Toggle_Break},
    {"EditDebugClear", CK_Debug_Clear},
    {"EditDebugNext", CK_Debug_Next},
    {"EditDebugStep", CK_Debug_Step},
    {"EditDebugBackTrace", CK_Debug_Back_Trace},
    {"EditDebugContinue", CK_Debug_Continue},
    {"EditDebugEnterCommand", CK_Debug_Enter_Command},
    {"EditDebugUntilCurser", CK_Debug_Until_Curser},
#endif
    {"EditInsertFile", CK_Insert_File},
    {"EditQuit", CK_Quit},
    {"EditToggleInsert", CK_Toggle_Insert},
    {"EditHelp", CK_Help},
    {"EditDate", CK_Date},
    {"EditRefresh", CK_Refresh},
    {"EditGoto", CK_Goto},
    {"EditDeleteLine", CK_Delete_Line},
    {"EditDeleteToLineEnd", CK_Delete_To_Line_End},
    {"EditDeleteToLineBegin", CK_Delete_To_Line_Begin},
    {"EditManPage", CK_Man_Page},
    {"EditSort", CK_Sort},
    {"EditMail", CK_Mail},
    {"EditCancel", CK_Cancel},
    {"EditComplete", CK_Complete},
    {"EditParagraphFormat", CK_Paragraph_Format},
    {"EditUtil", CK_Util},
    {"EditTypeLoadPython", CK_Type_Load_Python},
    {"EditFindFile", CK_Find_File},
    {"EditCtags", CK_Ctags},
    {"EditMatchBracket", CK_Match_Bracket},
    {"EditTerminal", CK_Terminal},
    {"EditTerminalApp", CK_Terminal_App},
    {"EditExtCmd", CK_ExtCmd},
    {"EditUserMenu", CK_User_Menu},
    {"EditBeginRecordMacro", CK_Begin_Record_Macro},
    {"EditEndRecordMacro", CK_End_Record_Macro},
    {"EditDeleteMacro", CK_Delete_Macro},
    {"EditToggleBookmark", CK_Toggle_Bookmark},
    {"EditFlushBookmarks", CK_Flush_Bookmarks},
    {"EditNextBookmark", CK_Next_Bookmark},
    {"EditPrevBookmark", CK_Prev_Bookmark},
    {"EditPageUpHighlight", CK_Page_Up_Highlight},
    {"EditPageDownHighlight", CK_Page_Down_Highlight},
    {"EditLeftHighlight", CK_Left_Highlight},
    {"EditRightHighlight", CK_Right_Highlight},
    {"EditWordLeftHighlight", CK_Word_Left_Highlight},
    {"EditWordRightHighlight", CK_Word_Right_Highlight},
    {"EditUpHighlight", CK_Up_Highlight},
    {"EditDownHighlight", CK_Down_Highlight},
    {"EditHomeHighlight", CK_Home_Highlight},
    {"EditEndHighlight", CK_End_Highlight},
    {"EditBeginningOfTextHighlight", CK_Beginning_Of_Text_Highlight},
    {"EditEndOfTextHighlight", CK_End_Of_Text_Highlight},
    {"EditBeginPageHighlight", CK_Begin_Page_Highlight},
    {"EditEndPageHighlight", CK_End_Page_Highlight},
    {"EditScrollUpHighlight", CK_Scroll_Up_Highlight},
    {"EditScrollDownHighlight", CK_Scroll_Down_Highlight},
    {"EditParagraphUpHighlight", CK_Paragraph_Up_Highlight},
    {"EditParagraphDownHighlight", CK_Paragraph_Down_Highlight},

    {"EditPageUpAltHighlight", CK_Page_Up_Alt_Highlight},
    {"EditPageDownAltHighlight", CK_Page_Down_Alt_Highlight},
    {"EditLeftAltHighlight", CK_Left_Alt_Highlight},
    {"EditRightAltHighlight", CK_Right_Alt_Highlight},
    {"EditWordLeftAltHighlight", CK_Word_Left_Alt_Highlight},
    {"EditWordRightAltHighlight", CK_Word_Right_Alt_Highlight},
    {"EditUpAltHighlight", CK_Up_Alt_Highlight},
    {"EditDownAltHighlight", CK_Down_Alt_Highlight},
    {"EditHomeAltHighlight", CK_Home_Alt_Highlight},
    {"EditEndAltHighlight", CK_End_Alt_Highlight},
    {"EditBeginningOfTextAltHighlight", CK_Beginning_Of_Text_Alt_Highlight},
    {"EditEndOfTextAltHighlight", CK_End_Of_Text_Alt_Highlight},
    {"EditBeginPageAltHighlight", CK_Begin_Page_Alt_Highlight},
    {"EditEndPageAltHighlight", CK_End_Page_Alt_Highlight},
    {"EditScrollUpAltHighlight", CK_Scroll_Up_Alt_Highlight},
    {"EditScrollDownAltHighlight", CK_Scroll_Down_Alt_Highlight},
    {"EditParagraphUpAltHighlight", CK_Paragraph_Up_Alt_Highlight},
    {"EditParagraphDownAltHighlight", CK_Paragraph_Down_Alt_Highlight},

    {"EditShiftBlockLeft", CK_Shift_Block_Left},
    {"EditShiftBlockRight", CK_Shift_Block_Right},

    {"EditXStore", CK_XStore},
    {"EditXCut", CK_XCut},
    {"EditXPaste", CK_XPaste},
    {"EditSelectionHistory", CK_Selection_History},
    {"EditShell", CK_Shell},
    {"EditInsertLiteral", CK_Insert_Literal},
    {"EditExecuteMacro", CK_Execute_Macro},
    {"EditBeginOrEndMacro", CK_Begin_End_Macro},
    {"EditExtMode", CK_Ext_Mode},
    {"EditToggleLineState", CK_Toggle_Line_State},
    {"EditToggleTabTWS", CK_Toggle_Tab_TWS},
    {"EditToggleSyntax", CK_Toggle_Syntax},
    {"EditToggleShowMargin", CK_Toggle_Show_Margin},
    {"EditFindDefinition", CK_Find_Definition},
    {"EditLoadPrevFile", CK_Load_Prev_File},
    {"EditLoadNextFile", CK_Load_Next_File},
    {"EditOptions", CK_Edit_Options},
    {"EditSaveMode", CK_Edit_Save_Mode},
    {"EditChooseSyntax", CK_Choose_Syntax},
    {"EditAbout", CK_About},

#if 0
    {"EditFocusNext", CK_Focus_Next},
    {"EditFocusPrev", CK_Focus_Prev},
    {"EditHeightInc", CK_Height_Inc},
    {"EditHeightDec", CK_Height_Dec},
    {"EditMake", CK_Make},
    {"EditErrorNext", CK_Error_Next},
    {"EditErrorPrev", CK_Error_Prev},
#endif

#if 0
    {"EditSaveDesktop", CK_Save_Desktop},
    {"EditNewWindow", CK_New_Window},
    {"EditCycle", CK_Cycle},
    {"EditSaveAndQuit", CK_Save_And_Quit},
    {"EditRunAnother", CK_Run_Another},
    {"EditCheckSaveAndQuit", CK_Check_Save_And_Quit},
    {"EditMaximize", CK_Maximize},
#endif

#endif /* USE_INTERNAL_EDIT */

    /* viewer */
    {"ViewHelp", CK_ViewHelp},
    {"ViewToggleWrapMode", CK_ViewToggleWrapMode},
    {"ViewToggleHexEditMode", CK_ViewToggleHexEditMode},
    {"ViewQuit", CK_ViewQuit},
    {"ViewToggleHexMode", CK_ViewToggleHexMode},
    {"ViewGoto", CK_ViewGoto},
    {"ViewHexEditSave", CK_ViewHexEditSave},
    {"ViewSearch", CK_ViewSearch},
    {"ViewToggleMagicMode", CK_ViewToggleMagicMode},
    {"ViewToggleNroffMode", CK_ViewToggleNroffMode},
    {"ViewContinueSearch", CK_ViewContinueSearch},
    {"ViewGotoBookmark", CK_ViewGotoBookmark},
    {"ViewNewBookmark", CK_ViewNewBookmark},
    {"ViewMoveUp", CK_ViewMoveUp},
    {"ViewMoveDown", CK_ViewMoveDown},
    {"ViewMoveLeft", CK_ViewMoveLeft},
    {"ViewMoveRight", CK_ViewMoveRight},
    {"ViewMoveLeft10", CK_ViewMoveLeft10},
    {"ViewMoveRight10", CK_ViewMoveRight10},
    {"ViewMovePgDn", CK_ViewMovePgDn},
    {"ViewMovePgUp", CK_ViewMovePgUp},
    {"ViewMoveHalfPgDn", CK_ViewMoveHalfPgDn},
    {"ViewMoveHalfPgUp", CK_ViewMoveHalfPgUp},
    {"ViewMoveToBol", CK_ViewMoveToBol},
    {"ViewMoveToEol", CK_ViewMoveToEol},
    {"ViewMoveTop", CK_ViewMoveTop},
    {"ViewMoveBottom", CK_ViewMoveBottom},
    {"ViewNextFile", CK_ViewNextFile},
    {"ViewPrevFile", CK_ViewPrevFile},
    {"ViewToggleRuler", CK_ViewToggleRuler},
    {"ViewToggleHexNavMode", CK_ViewToggleHexNavMode},

    /* help */
    {"HelpHelp", CK_HelpHelp},
    {"HelpIndex", CK_HelpIndex},
    {"HelpBack", CK_HelpBack},
    {"HelpQuit", CK_HelpQuit},
    {"HelpMoveUp", CK_HelpMoveUp},
    {"HelpMoveDown", CK_HelpMoveDown},
    {"HelpMovePgDn", CK_HelpMovePgDn},
    {"HelpMovePgUp", CK_HelpMovePgUp},
    {"HelpMoveHalfPgDn", CK_HelpMoveHalfPgDn},
    {"HelpMoveHalfPgUp", CK_HelpMoveHalfPgUp},
    {"HelpMoveTop", CK_HelpMoveTop},
    {"HelpMoveBottom", CK_HelpMoveBottom},
    {"HelpSelectLink", CK_HelpSelectLink},
    {"HelpNextLink", CK_HelpNextLink},
    {"HelpPrevLink", CK_HelpPrevLink},
    {"HelpNextNode", CK_HelpNextNode},
    {"HelpPrevNode", CK_HelpPrevNode},

    /* tree */
    {"TreeHelp", CK_TreeHelp},
    {"TreeForget", CK_TreeForget},
    {"TreeToggleNav", CK_TreeToggleNav},
    {"TreeCopy", CK_TreeCopy},
    {"TreeMove", CK_TreeMove},
    {"TreeMake", CK_TreeMake},
    {"TreeMoveUp", CK_TreeMoveUp},
    {"TreeMoveDown", CK_TreeMoveDown},
    {"TreeMoveLeft", CK_TreeMoveLeft},
    {"TreeMoveRight", CK_TreeMoveRight},
    {"TreeMoveHome", CK_TreeMoveHome},
    {"TreeMoveEnd", CK_TreeMoveEnd},
    {"TreeMovePgUp", CK_TreeMovePgUp},
    {"TreeMovePgDn", CK_TreeMovePgDn},
    {"TreeOpen", CK_TreeOpen},
    {"TreeRescan", CK_TreeRescan},
    {"TreeStartSearch", CK_TreeStartSearch},
    {"TreeRemove", CK_TreeRemove},

    /* main commands */
    {"CmdHelp", CK_HelpCmd},
    {"CmdMenu", CK_MenuCmd},
    {"CmdChmod", CK_ChmodCmd},
    {"CmdMenuLastSelected", CK_MenuLastSelectedCmd},
    {"CmdSingleDirsize", CK_SingleDirsizeCmd},
    {"CmdCopyCurrentPathname", CK_CopyCurrentPathname},
    {"CmdCopyOtherPathname", CK_CopyOtherPathname},
    {"CmdSuspend", CK_SuspendCmd},
    {"CmdToggleListing", CK_ToggleListingCmd},
    {"CmdChownAdvanced", CK_ChownAdvancedCmd},
    {"CmdChown", CK_ChownCmd},
    {"CmdCompareDirs", CK_CompareDirsCmd},
    {"CmdConfigureBox", CK_ConfigureBox},
    {"CmdConfigureVfs", CK_ConfigureVfs},
    {"CmdConfirmBox", CK_ConfirmBox},
    {"CmdCopy", CK_CopyCmd},
    {"CmdDelete", CK_DeleteCmd},
    {"CmdDirsizes", CK_DirsizesCmd},
    {"CmdDisplayBitsBox", CK_DisplayBitsBox},
    {"CmdEdit", CK_EditCmd},
#ifdef USE_INTERNAL_EDIT
    {"CmdEditForceInternal", CK_EditForceInternalCmd},
#endif
    {"CmdEditExtFile", CK_EditExtFileCmd},
    {"CmdEditFhlFile", CK_EditFhlFileCmd},
    {"CmdEditMcMenu", CK_EditMcMenuCmd},
    {"CmdEditSymlink", CK_EditSymlinkCmd},
    {"CmdEditSyntax", CK_EditSyntaxCmd},
    {"CmdEditUserMenu", CK_EditUserMenuCmd},
    {"CmdExternalPanelize", CK_ExternalPanelize},
    {"CmdFilter", CK_FilterCmd},
    {"CmdFilteredView", CK_FilteredViewCmd},
    {"CmdFind", CK_FindCmd},
#ifdef ENABLE_VFS_FISH
    {"CmdFishlink", CK_FishlinkCmd},
#endif
#ifdef ENABLE_VFS_FTP
    {"CmdFtplink", CK_FtplinkCmd},
#endif
    {"CmdHistory", CK_HistoryCmd},
    {"CmdInfo", CK_InfoCmd},
#ifdef WITH_BACKGROUND
    {"CmdJobs", CK_JobsCmd},
#endif
    {"CmdLayout", CK_LayoutBox},
    {"CmdLearnKeys", CK_LearnKeys},
    {"CmdLink", CK_LinkCmd},
    {"CmdChangeListing", CK_ChangeListingCmd},
    {"CmdListing", CK_ListingCmd},
#ifdef LISTMODE_EDITOR
    {"CmdListmodeCmd", CK_ListmodeCmd}.
#endif
    {"CmdMkdir", CK_MkdirCmd},
    {"CmdPanelOptions", CK_PanelOptionsBox},
    {"CmdQuickCd", CK_QuickCdCmd},
    {"CmdQuickChdir", CK_QuickChdirCmd},
    {"CmdQuickView", CK_QuickViewCmd},
    {"CmdQuietQuit", CK_QuietQuitCmd},
    {"CmdRelativeSymlink", CK_RelativeSymlinkCmd},
    {"CmdRename", CK_RenameCmd},
    {"CmdReread", CK_RereadCmd},
    {"CmdReselectVfs", CK_ReselectVfs},
    {"CmdReverseSelection", CK_ReverseSelectionCmd},
    {"CmdSaveSetup", CK_SaveSetupCmd},
    {"CmdSelect", CK_SelectCmd},
#ifdef ENABLE_VFS_SMB
    {"CmdSmblinkCmd", CK_SmblinkCmd},
#endif
    {"CmdSwapPanel", CK_SwapCmd},
    {"CmdSymlink", CK_SymlinkCmd},
    {"CmdTree", CK_TreeCmd},
    {"CmdTreeBox", CK_TreeBoxCmd},
#ifdef ENABLE_VFS_UNDELFS
    {"CmdUndelete", CK_UndeleteCmd},
#endif
    {"CmdUnselect", CK_UnselectCmd},
    {"CmdUserMenu", CK_UserMenuCmd},
    {"CmdUserFileMenu", CK_UserFileMenuCmd},
    {"CmdView", CK_ViewCmd},
    {"CmdViewFile", CK_ViewFileCmd},
    {"CmdCopyCurrentReadlink", CK_CopyCurrentReadlink},
    {"CmdCopyOtherReadlink", CK_CopyOtherReadlink},
    {"CmdAddHotlist", CK_AddHotlist},
    {"CmdQuit", CK_QuitCmd},
    {"CmdCopyCurrentTagged", CK_CopyCurrentTagged},
    {"CmdCopyOtherTagged", CK_CopyOtherTagged},
    {"CmdToggleShowHidden", CK_ToggleShowHidden},
    {"CmdTogglePanelsSplit", CK_TogglePanelsSplit},
#ifdef USE_DIFF_VIEW
    {"CmdDiffView", CK_DiffViewCmd},
#endif
    {"CmdDialogList", CK_DialogListCmd},
    {"CmdDialogNext", CK_DialogNextCmd},
    {"CmdDialogPrev", CK_DialogPrevCmd},

    /* panel */
    {"PanelChdirOtherPanel", CK_PanelChdirOtherPanel},
    {"PanelChdirToReadlink", CK_PanelChdirToReadlink},
    {"PanelCopyLocal", CK_PanelCmdCopyLocal},
    {"PanelDeleteLocal", CK_PanelCmdDeleteLocal},
    {"PanelDoEnter", CK_PanelCmdDoEnter},
    {"PanelEditNew", CK_PanelCmdEditNew},
    {"PanelRenameLocal", CK_PanelCmdRenameLocal},
    {"PanelReverseSelection", CK_PanelCmdReverseSelection},
    {"PanelSelect", CK_PanelCmdSelect},
    {"PanelUnselect", CK_PanelCmdUnselect},
    {"PanelViewSimple", CK_PanelCmdViewSimple},
    {"PanelGotoParentDir", CK_PanelGotoParentDir},
    {"PanelGotoChildDir", CK_PanelGotoChildDir},
    {"PanelDirectoryHistoryList", CK_PanelDirectoryHistoryList},
    {"PanelDirectoryHistoryNext", CK_PanelDirectoryHistoryNext},
    {"PanelDirectoryHistoryPrev", CK_PanelDirectoryHistoryPrev},
    {"PanelGotoBottomFile", CK_PanelGotoBottomFile},
    {"PanelGotoMiddleFile", CK_PanelGotoMiddleFile},
    {"PanelGotoTopFile", CK_PanelGotoTopFile},
    {"PanelMarkFile", CK_PanelMarkFile},
    {"PanelMarkFileDown", CK_PanelMarkFileDown},
    {"PanelMarkFileUp", CK_PanelMarkFileUp},
    {"PanelMoveUp", CK_PanelMoveUp},
    {"PanelMoveDown", CK_PanelMoveDown},
    {"PanelMoveLeft", CK_PanelMoveLeft},
    {"PanelMoveRight", CK_PanelMoveRight},
    {"PanelMoveEnd", CK_PanelMoveEnd},
    {"PanelMoveHome", CK_PanelMoveHome},
    {"PanelNextPage", CK_PanelNextPage},
    {"PanelPrevPage", CK_PanelPrevPage},
#ifdef HAVE_CHARSET
    {"PanelSetPanelEncoding", CK_PanelSetPanelEncoding},
#endif
    {"PanelStartSearch", CK_PanelStartSearch},
    {"PanelSyncOtherPanel", CK_PanelSyncOtherPanel},
    {"PanelToggleSortOrderNext", CK_PanelToggleSortOrderNext},
    {"PanelToggleSortOrderPrev", CK_PanelToggleSortOrderPrev},
    {"PanelSelectSortOrder", CK_PanelSelectSortOrder},
    {"PanelReverseSort", CK_PanelReverseSort},
    {"PanelSortOrderByName", CK_PanelSortOrderByName},
    {"PanelSortOrderByExt", CK_PanelSortOrderByExt},
    {"PanelSortOrderBySize", CK_PanelSortOrderBySize},
    {"PanelSortOrderByMTime", CK_PanelSortOrderByMTime},
    {"PanelSmartGotoParentDir", CK_PanelSmartGotoParentDir},

    /* input line */
    {"InputBol", CK_InputBol},
    {"InputEol", CK_InputEol},
    {"InputMoveLeft", CK_InputMoveLeft},
    {"InputWordLeft", CK_InputWordLeft},
    {"InputBackwardChar", CK_InputBackwardChar},
    {"InputBackwardWord", CK_InputBackwardWord},
    {"InputMoveRight", CK_InputMoveRight},
    {"InputWordRight", CK_InputWordRight},
    {"InputForwardChar", CK_InputForwardChar},
    {"InputForwardWord", CK_InputForwardWord},
    {"InputBackwardDelete", CK_InputBackwardDelete},
    {"InputDeleteChar", CK_InputDeleteChar},
    {"InputKillWord", CK_InputKillWord},
    {"InputBackwardKillWord", CK_InputBackwardKillWord},
    {"InputSetMark", CK_InputSetMark},
    {"InputKillRegion", CK_InputKillRegion},
    {"InputYank", CK_InputYank},
    {"InputKillLine", CK_InputKillLine},
    {"InputHistoryPrev", CK_InputHistoryPrev},
    {"InputHistoryNext", CK_InputHistoryNext},
    {"InputHistoryShow", CK_InputHistoryShow},
    {"InputComplete", CK_InputComplete},
    {"InputXStore", CK_InputKillSave},
    {"InputXPaste", CK_InputPaste},
    {"InputClearLine", CK_InputClearLine},
    {"InputLeftHighlight", CK_InputLeftHighlight},
    {"InputRightHighlight", CK_InputRightHighlight},
    {"InputWordLeftHighlight", CK_InputWordLeftHighlight},
    {"InputWordRightHighlight", CK_InputWordRightHighlight},
    {"InputBolHighlight", CK_InputBolHighlight},
    {"InputEolHighlight", CK_InputEolHighlight},

    /* listbox */
    {"ListboxMoveUp", CK_ListboxMoveUp},
    {"ListboxMoveDown", CK_ListboxMoveDown},
    {"ListboxMoveHome", CK_ListboxMoveHome},
    {"ListboxMoveEnd", CK_ListboxMoveEnd},
    {"ListboxMovePgUp", CK_ListboxMovePgUp},
    {"ListboxMovePgDn", CK_ListboxMovePgDn},
    {"ListboxDeleteItem", CK_ListboxDeleteItem},
    {"ListboxDeleteAll", CK_ListboxDeleteAll},

    /* common */
    {"ExtMap1", CK_StartExtMap1},
    {"ExtMap2", CK_StartExtMap2},
    {"ShowCommandLine", CK_ShowCommandLine},
    {"SelectCodepage", CK_SelectCodepage},

    /* dialog */
    {"DialogOK", CK_DialogOK},
    {"DialogCancel", CK_DialogCancel},
    {"DialogPrevItem", CK_DialogPrevItem},
    {"DialogNextItem", CK_DialogNextItem},
    {"DialogHelp", CK_DialogHelp},
    {"DialogSuspend", CK_DialogSuspend},
    {"DialogRefresh", CK_DialogRefresh},

#ifdef  USE_DIFF_VIEW
    /* diff viewer */
    {"DiffDisplaySymbols", CK_DiffDisplaySymbols},
    {"DiffDisplayNumbers", CK_DiffDisplayNumbers},
    {"DiffFull", CK_DiffFull},
    {"DiffEqual", CK_DiffEqual},
    {"DiffSplitMore", CK_DiffSplitMore},
    {"DiffSplitLess", CK_DiffSplitLess},
    {"DiffShowDiff", CK_DiffShowDiff},
    {"DiffSetTab2", CK_DiffSetTab2},
    {"DiffSetTab3", CK_DiffSetTab3},
    {"DiffSetTab4", CK_DiffSetTab4},
    {"DiffSetTab8", CK_DiffSetTab8},
    {"DiffSwapPanel", CK_DiffSwapPanel},
    {"DiffRedo", CK_DiffRedo},
    {"DiffNextHunk", CK_DiffNextHunk},
    {"DiffPrevHunk", CK_DiffPrevHunk},
    {"DiffGoto", CK_DiffGoto},
    {"DiffEditCurrent", CK_DiffEditCurrent},
    {"DiffEditOther", CK_DiffEditOther},
    {"DiffSearch", CK_DiffSearch},
    {"DiffContinueSearch", CK_DiffContinueSearch},
    {"DiffEOF", CK_DiffEOF},
    {"DiffBOF", CK_DiffBOF},
    {"DiffDown", CK_DiffDown},
    {"DiffUp", CK_DiffUp},
    {"DiffQuickLeft", CK_DiffQuickLeft},
    {"DiffQuickRight", CK_DiffQuickRight},
    {"DiffLeft", CK_DiffLeft},
    {"DiffRight", CK_DiffRight},
    {"DiffPageDown", CK_DiffPageDown},
    {"DiffPageUp", CK_DiffPageUp},
    {"DiffHome", CK_DiffHome},
    {"DiffEnd", CK_DiffEnd},
    {"DiffQuit", CK_DiffQuit},
    {"DiffHelp", CK_DiffHelp},
    {"SelectCodepage", CK_SelectCodepage},
    {"DiffMergeCurrentHunk", CK_DiffMergeCurrentHunk},
    {"DiffSave", CK_DiffSave},
    {"DiffOptions", CK_DiffOptions},
#endif

    {NULL, CK_Ignore_Key}
};

static const size_t num_command_names = sizeof (command_names) / sizeof (command_names[0]) - 1;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
name_keymap_comparator (const void *p1, const void *p2)
{
    const name_keymap_t *m1 = (const name_keymap_t *) p1;
    const name_keymap_t *m2 = (const name_keymap_t *) p2;

    return str_casecmp (m1->name, m2->name);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
sort_command_names (void)
{
    static gboolean has_been_sorted = FALSE;

    if (!has_been_sorted)
    {
        qsort (command_names, num_command_names,
               sizeof (command_names[0]), &name_keymap_comparator);
        has_been_sorted = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
keymap_add (GArray * keymap, long key, unsigned long cmd, const char *caption)
{
    if (key != 0 && cmd != CK_Ignore_Key)
    {
        global_keymap_t new_bind;

        new_bind.key = key;
        new_bind.command = cmd;
        g_snprintf (new_bind.caption, sizeof (new_bind.caption), "%s", caption);
        g_array_append_val (keymap, new_bind);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
keybind_cmd_bind (GArray * keymap, const char *keybind, unsigned long action)
{
    char *caption = NULL;
    long key;

    key = lookup_key (keybind, &caption);
    keymap_add (keymap, key, action, caption);
    g_free (caption);
}

/* --------------------------------------------------------------------------------------------- */

unsigned long
keybind_lookup_action (const char *name)
{
    const name_keymap_t key = { name, 0 };
    name_keymap_t *res;

    sort_command_names ();

    res = bsearch (&key, command_names, num_command_names,
                   sizeof (command_names[0]), name_keymap_comparator);

    return (res != NULL) ? res->val : CK_Ignore_Key;
}

/* --------------------------------------------------------------------------------------------- */

const char *
keybind_lookup_keymap_shortcut (const global_keymap_t * keymap, unsigned long action)
{
    size_t i;

    for (i = 0; keymap[i].key != 0; i++)
        if (keymap[i].command == action)
            return (keymap[i].caption[0] != '\0') ? keymap[i].caption : NULL;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

unsigned long
keybind_lookup_keymap_command (const global_keymap_t * keymap, long key)
{
    size_t i;

    for (i = 0; keymap[i].key != 0; i++)
        if (keymap[i].key == key)
            return keymap[i].command;

    return CK_Ignore_Key;
}

/* --------------------------------------------------------------------------------------------- */
