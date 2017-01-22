/*
   Definitions of key bindings.

   Copyright (C) 2005-2017
   Free Software Foundation, Inc.

   Written by:
   Vitja Makarov, 2005
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2012
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/tty/key.h"        /* KEY_M_ */
#include "lib/keybind.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static name_keymap_t command_names[] = {
    /* common */
    {"InsertChar", CK_InsertChar},
    {"Enter", CK_Enter},
    {"ChangePanel", CK_ChangePanel},
    {"Up", CK_Up},
    {"Down", CK_Down},
    {"Left", CK_Left},
    {"Right", CK_Right},
    {"LeftQuick", CK_LeftQuick},
    {"RightQuick", CK_RightQuick},
    {"Home", CK_Home},
    {"End", CK_End},
    {"PageUp", CK_PageUp},
    {"PageDown", CK_PageDown},
    {"HalfPageUp", CK_HalfPageUp},
    {"HalfPageDown", CK_HalfPageDown},
    {"Top", CK_Top},
    {"Bottom", CK_Bottom},
    {"TopOnScreen", CK_TopOnScreen},
    {"MiddleOnScreen", CK_MiddleOnScreen},
    {"BottomOnScreen", CK_BottomOnScreen},
    {"WordLeft", CK_WordLeft},
    {"WordRight", CK_WordRight},
    {"Copy", CK_Copy},
    {"Move", CK_Move},
    {"Delete", CK_Delete},
    {"MakeDir", CK_MakeDir},
    {"ChangeMode", CK_ChangeMode},
    {"ChangeOwn", CK_ChangeOwn},
    {"ChangeOwnAdvanced", CK_ChangeOwnAdvanced},
    {"Remove", CK_Remove},
    {"BackSpace", CK_BackSpace},
    {"Redo", CK_Redo},
    {"Clear", CK_Clear},
    {"Menu", CK_Menu},
    {"MenuLastSelected", CK_MenuLastSelected},
    {"UserMenu", CK_UserMenu},
    {"EditUserMenu", CK_EditUserMenu},
    {"Search", CK_Search},
    {"SearchContinue", CK_SearchContinue},
    {"Replace", CK_Replace},
    {"ReplaceContinue", CK_ReplaceContinue},
    {"Help", CK_Help},
    {"Shell", CK_Shell},
    {"Edit", CK_Edit},
    {"EditNew", CK_EditNew},
#ifdef HAVE_CHARSET
    {"SelectCodepage", CK_SelectCodepage},
#endif
    {"History", CK_History},
    {"HistoryNext", CK_HistoryNext},
    {"HistoryPrev", CK_HistoryPrev},
    {"Complete", CK_Complete},
    {"Save", CK_Save},
    {"SaveAs", CK_SaveAs},
    {"Goto", CK_Goto},
    {"Reread", CK_Reread},
    {"Refresh", CK_Refresh},
    {"Suspend", CK_Suspend},
    {"Swap", CK_Swap},
    {"HotList", CK_HotList},
    {"SelectInvert", CK_SelectInvert},
    {"ScreenList", CK_ScreenList},
    {"ScreenNext", CK_ScreenNext},
    {"ScreenPrev", CK_ScreenPrev},
    {"FileNext", CK_FileNext},
    {"FilePrev", CK_FilePrev},
    {"DeleteToHome", CK_DeleteToHome},
    {"DeleteToEnd", CK_DeleteToEnd},
    {"DeleteToWordBegin", CK_DeleteToWordBegin},
    {"DeleteToWordEnd", CK_DeleteToWordEnd},
    {"Cut", CK_Cut},
    {"Store", CK_Store},
    {"Paste", CK_Paste},
    {"Mark", CK_Mark},
    {"MarkLeft", CK_MarkLeft},
    {"MarkRight", CK_MarkRight},
    {"MarkUp", CK_MarkUp},
    {"MarkDown", CK_MarkDown},
    {"MarkToWordBegin", CK_MarkToWordBegin},
    {"MarkToWordEnd", CK_MarkToWordEnd},
    {"MarkToHome", CK_MarkToHome},
    {"MarkToEnd", CK_MarkToEnd},
    {"ToggleNavigation", CK_ToggleNavigation},
    {"Sort", CK_Sort},
    {"Options", CK_Options},
    {"LearnKeys", CK_LearnKeys},
    {"Bookmark", CK_Bookmark},
    {"Quit", CK_Quit},
    {"QuitQuiet", CK_QuitQuiet},
    {"ExtendedKeyMap", CK_ExtendedKeyMap},

    /* main commands */
#ifdef USE_INTERNAL_EDIT
    {"EditForceInternal", CK_EditForceInternal},
#endif
    {"View", CK_View},
    {"ViewRaw", CK_ViewRaw},
    {"ViewFile", CK_ViewFile},
    {"ViewFiltered", CK_ViewFiltered},
    {"Find", CK_Find},
    {"DirSize", CK_DirSize},
    {"PanelListingSwitch", CK_PanelListingSwitch},
    {"CompareDirs", CK_CompareDirs},
#ifdef USE_DIFF_VIEW
    {"CompareFiles", CK_CompareFiles},
#endif
    {"OptionsVfs", CK_OptionsVfs},
    {"OptionsConfirm", CK_OptionsConfirm},
    {"OptionsDisplayBits", CK_OptionsDisplayBits},
    {"EditExtensionsFile", CK_EditExtensionsFile},
    {"EditFileHighlightFile", CK_EditFileHighlightFile},
    {"LinkSymbolicEdit", CK_LinkSymbolicEdit},
    {"ExternalPanelize", CK_ExternalPanelize},
    {"Filter", CK_Filter},
#ifdef ENABLE_VFS_FISH
    {"ConnectFish", CK_ConnectFish},
#endif
#ifdef ENABLE_VFS_FTP
    {"ConnectFtp", CK_ConnectFtp},
#endif
#ifdef ENABLE_VFS_SFTP
    {"ConnectSftp", CK_ConnectSftp},
#endif
#ifdef ENABLE_VFS_SMB
    {"ConnectSmb", CK_ConnectSmb},
#endif
    {"PanelInfo", CK_PanelInfo},
#ifdef ENABLE_BACKGROUND
    {"Jobs", CK_Jobs},
#endif
    {"OptionsLayout", CK_OptionsLayout},
    {"OptionsAppearance", CK_OptionsAppearance},
    {"Link", CK_Link},
    {"PanelListingChange", CK_PanelListingChange},
    {"PanelListing", CK_PanelListing},
#ifdef LISTMODE_EDITOR
    {"ListMode", CK_ListMode}.
#endif
    {"OptionsPanel", CK_OptionsPanel},
    {"CdQuick", CK_CdQuick},
    {"PanelQuickView", CK_PanelQuickView},
    {"LinkSymbolicRelative", CK_LinkSymbolicRelative},
    {"VfsList", CK_VfsList},
    {"SaveSetup", CK_SaveSetup},
    {"LinkSymbolic", CK_LinkSymbolic},
    {"PanelTree", CK_PanelTree},
    {"Tree", CK_Tree},
#ifdef ENABLE_VFS_UNDELFS
    {"Undelete", CK_Undelete},
#endif
    {"PutCurrentLink", CK_PutCurrentLink},
    {"PutOtherLink", CK_PutOtherLink},
    {"HotListAdd", CK_HotListAdd},
    {"ShowHidden", CK_ShowHidden},
    {"SplitVertHoriz", CK_SplitVertHoriz},
    {"SplitEqual", CK_SplitEqual},
    {"SplitMore", CK_SplitMore},
    {"SplitLess", CK_SplitLess},
    {"PutCurrentPath", CK_PutCurrentPath},
    {"PutOtherPath", CK_PutOtherPath},
    {"PutCurrentSelected", CK_PutCurrentSelected},
    {"PutCurrentFullSelected", CK_PutCurrentFullSelected},
    {"PutCurrentTagged", CK_PutCurrentTagged},
    {"PutOtherTagged", CK_PutOtherTagged},
    {"Select", CK_Select},
    {"Unselect", CK_Unselect},

    /* panel */
    {"SelectExt", CK_SelectExt},
    {"ScrollLeft", CK_ScrollLeft},
    {"ScrollRight", CK_ScrollRight},
    {"PanelOtherCd", CK_PanelOtherCd},
    {"PanelOtherCdLink", CK_PanelOtherCdLink},
    {"CopySingle", CK_CopySingle},
    {"MoveSingle", CK_MoveSingle},
    {"DeleteSingle", CK_DeleteSingle},
    {"CdParent", CK_CdParent},
    {"CdChild", CK_CdChild},
    {"Panelize", CK_Panelize},
    {"PanelOtherSync", CK_PanelOtherSync},
    {"SortNext", CK_SortNext},
    {"SortPrev", CK_SortPrev},
    {"SortReverse", CK_SortReverse},
    {"SortByName", CK_SortByName},
    {"SortByExt", CK_SortByExt},
    {"SortBySize", CK_SortBySize},
    {"SortByMTime", CK_SortByMTime},
    {"CdParentSmart", CK_CdParentSmart},

    /* dialog */
    {"Ok", CK_Ok},
    {"Cancel", CK_Cancel},

    /* input line */
    {"Yank", CK_Yank},

    /* help */
    {"Index", CK_Index},
    {"Back", CK_Back},
    {"LinkNext", CK_LinkNext},
    {"LinkPrev", CK_LinkPrev},
    {"NodeNext", CK_NodeNext},
    {"NodePrev", CK_NodePrev},

    /* tree */
    {"Forget", CK_Forget},

#if defined (USE_INTERNAL_EDIT) || defined (USE_DIFF_VIEW)
    {"ShowNumbers", CK_ShowNumbers},
#endif

#ifdef USE_INTERNAL_EDIT
    {"Close", CK_Close},
    {"Tab", CK_Tab},
    {"Undo", CK_Undo},
    {"ScrollUp", CK_ScrollUp},
    {"ScrollDown", CK_ScrollDown},
    {"Return", CK_Return},
    {"ParagraphUp", CK_ParagraphUp},
    {"ParagraphDown", CK_ParagraphDown},
    {"EditFile", CK_EditFile},
    {"MarkWord", CK_MarkWord},
    {"MarkLine", CK_MarkLine},
    {"MarkAll", CK_MarkAll},
    {"Unmark", CK_Unmark},
    {"MarkColumn", CK_MarkColumn},
    {"BlockSave", CK_BlockSave},
    {"InsertFile", CK_InsertFile},
    {"InsertOverwrite", CK_InsertOverwrite},
    {"Date", CK_Date},
    {"DeleteLine", CK_DeleteLine},
    {"EditMail", CK_Mail},
    {"ParagraphFormat", CK_ParagraphFormat},
    {"MatchBracket", CK_MatchBracket},
    {"ExternalCommand", CK_ExternalCommand},
    {"MacroStartRecord", CK_MacroStartRecord},
    {"MacroStopRecord", CK_MacroStopRecord},
    {"MacroStartStopRecord", CK_MacroStartStopRecord},
    {"MacroDelete", CK_MacroDelete},
    {"RepeatStartStopRecord", CK_RepeatStartStopRecord},
#ifdef HAVE_ASPELL
    {"SpellCheck", CK_SpellCheck},
    {"SpellCheckCurrentWord", CK_SpellCheckCurrentWord},
    {"SpellCheckSelectLang", CK_SpellCheckSelectLang},
#endif /* HAVE_ASPELL */
    {"BookmarkFlush", CK_BookmarkFlush},
    {"BookmarkNext", CK_BookmarkNext},
    {"BookmarkPrev", CK_BookmarkPrev},
    {"MarkPageUp", CK_MarkPageUp},
    {"MarkPageDown", CK_MarkPageDown},
    {"MarkToFileBegin", CK_MarkToFileBegin},
    {"MarkToFileEnd", CK_MarkToFileEnd},
    {"MarkToPageBegin", CK_MarkToPageBegin},
    {"MarkToPageEnd", CK_MarkToPageEnd},
    {"MarkScrollUp", CK_MarkScrollUp},
    {"MarkScrollDown", CK_MarkScrollDown},
    {"MarkParagraphUp", CK_MarkParagraphUp},
    {"MarkParagraphDown", CK_MarkParagraphDown},
    {"MarkColumnPageUp", CK_MarkColumnPageUp},
    {"MarkColumnPageDown", CK_MarkColumnPageDown},
    {"MarkColumnLeft", CK_MarkColumnLeft},
    {"MarkColumnRight", CK_MarkColumnRight},
    {"MarkColumnUp", CK_MarkColumnUp},
    {"MarkColumnDown", CK_MarkColumnDown},
    {"MarkColumnScrollUp", CK_MarkColumnScrollUp},
    {"MarkColumnScrollDown", CK_MarkColumnScrollDown},
    {"MarkColumnParagraphUp", CK_MarkColumnParagraphUp},
    {"MarkColumnParagraphDown", CK_MarkColumnParagraphDown},
    {"BlockShiftLeft", CK_BlockShiftLeft},
    {"BlockShiftRight", CK_BlockShiftRight},
    {"InsertLiteral", CK_InsertLiteral},
    {"ShowTabTws", CK_ShowTabTws},
    {"SyntaxOnOff", CK_SyntaxOnOff},
    {"SyntaxChoose", CK_SyntaxChoose},
    {"ShowMargin", CK_ShowMargin},
    {"OptionsSaveMode", CK_OptionsSaveMode},
    {"About", CK_About},
    /* An action to run external script from macro */
    {"ExecuteScript", CK_PipeBlock (0)},
    {"WindowMove", CK_WindowMove},
    {"WindowResize", CK_WindowResize},
    {"WindowFullscreen", CK_WindowFullscreen},
    {"WindowList", CK_WindowList},
    {"WindowNext", CK_WindowNext},
    {"WindowPrev", CK_WindowPrev},
#endif /* USE_INTERNAL_EDIT */

    /* viewer */
    {"WrapMode", CK_WrapMode},
    {"HexEditMode", CK_HexEditMode},
    {"HexMode", CK_HexMode},
    {"MagicMode", CK_MagicMode},
    {"NroffMode", CK_NroffMode},
    {"BookmarkGoto", CK_BookmarkGoto},
    {"Ruler", CK_Ruler},
    {"SearchForward", CK_SearchForward},
    {"SearchBackward", CK_SearchBackward},
    {"SearchForwardContinue", CK_SearchForwardContinue},
    {"SearchBackwardContinue", CK_SearchBackwardContinue},

#ifdef USE_DIFF_VIEW
    /* diff viewer */
    {"ShowSymbols", CK_ShowSymbols},
    {"SplitFull", CK_SplitFull},
    {"Tab2", CK_Tab2},
    {"Tab3", CK_Tab3},
    {"Tab4", CK_Tab4},
    {"Tab8", CK_Tab8},
    {"HunkNext", CK_HunkNext},
    {"HunkPrev", CK_HunkPrev},
    {"EditOther", CK_EditOther},
    {"Merge", CK_Merge},
    {"MergeOther", CK_MergeOther},
#endif /* USE_DIFF_VIEW */

    {NULL, CK_IgnoreKey}
};

/* *INDENT-OFF* */
static const size_t num_command_names = G_N_ELEMENTS (command_names) - 1;
/* *INDENT-ON* */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
name_keymap_comparator (const void *p1, const void *p2)
{
    const name_keymap_t *m1 = (const name_keymap_t *) p1;
    const name_keymap_t *m2 = (const name_keymap_t *) p2;

    return g_ascii_strcasecmp (m1->name, m2->name);
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
keymap_add (GArray * keymap, long key, long cmd, const char *caption)
{
    if (key != 0 && cmd != CK_IgnoreKey)
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
keybind_cmd_bind (GArray * keymap, const char *keybind, long action)
{
    char *caption = NULL;
    long key;

    key = lookup_key (keybind, &caption);
    keymap_add (keymap, key, action, caption);
    g_free (caption);
}

/* --------------------------------------------------------------------------------------------- */

long
keybind_lookup_action (const char *name)
{
    const name_keymap_t key = { name, 0 };
    name_keymap_t *res;

    sort_command_names ();

    res = bsearch (&key, command_names, num_command_names,
                   sizeof (command_names[0]), name_keymap_comparator);

    return (res != NULL) ? res->val : CK_IgnoreKey;
}

/* --------------------------------------------------------------------------------------------- */

const char *
keybind_lookup_actionname (long action)
{
    size_t i;

    for (i = 0; command_names[i].name != NULL; i++)
        if (command_names[i].val == action)
            return command_names[i].name;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

const char *
keybind_lookup_keymap_shortcut (const global_keymap_t * keymap, long action)
{
    if (keymap != NULL)
    {
        size_t i;

        for (i = 0; keymap[i].key != 0; i++)
            if (keymap[i].command == action)
                return (keymap[i].caption[0] != '\0') ? keymap[i].caption : NULL;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

long
keybind_lookup_keymap_command (const global_keymap_t * keymap, long key)
{
    if (keymap != NULL)
    {
        size_t i;

        for (i = 0; keymap[i].key != 0; i++)
            if (keymap[i].key == key)
                return keymap[i].command;
    }

    return CK_IgnoreKey;
}

/* --------------------------------------------------------------------------------------------- */
