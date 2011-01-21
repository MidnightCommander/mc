#ifndef MC__KEYBIND_H
#define MC__KEYBIND_H

#include <sys/types.h>
#include <sys/time.h>           /* time_t */

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define KEYMAP_SHORTCUT_LENGTH 32       /* FIXME: is 32 bytes enough for shortcut? */

/* special commands */
#define CK_Insert_Char -1
#define CK_Ignore_Key   0

/* common */
#define CK_Enter             3
#define CK_Up                10
#define CK_Down              11
#define CK_Left              6
#define CK_Right             7
#define CK_Home              12
#define CK_End               13
#define CK_LeftQuick         9027
#define CK_RightQuick        9028
#define CK_PageUp            4
#define CK_PageDown          5
#define CK_HalfPageUp        1015
#define CK_HalfPageDown      1014
#define CK_Top               17
#define CK_Bottom            18
#define CK_TopOnScreen       8019
#define CK_MiddleOnScreen    8018
#define CK_BottomOnScreen    8017
#define CK_WordLeft          8
#define CK_WordRight         9
#define CK_Copy              202
#define CK_Move              203
#define CK_Delete            2
#define CK_MakeDir           7045
#define CK_ChangeMode        7003
#define CK_ChangeOwn         7005
#define CK_ChangeOwnAdvanced 7004
#define CK_Remove            204
#define CK_BackSpace         1
#define CK_Redo              16
#define CK_Clear             4026
#define CK_Menu              123
#define CK_MenuLastSelected  7044
#define CK_UserMenu          425
#define CK_EditUserMenu                 7026
#define CK_Search                       8032
#define CK_SearchContinue               5011
#define CK_Replace           303
#define CK_ReplaceContinue   304
#define CK_SearchStop                   8033
#define CK_Help                         404
#define CK_Edit                         7020
#define CK_Shell           801
#ifdef HAVE_CHARSET
#define CK_SelectCodepage               2002
#endif
#define CK_History                      4021
#define CK_HistoryNext                  4022
#define CK_HistoryPrev                  4023
#define CK_Complete                     305
#define CK_SaveAs                       104
#define CK_Goto                         407
#define CK_Reread                       7053
#define CK_Refresh                      406
#define CK_Suspend                      7060
#define CK_Swap                         7061
#define CK_HotList                      7048
#define CK_ScreenList                   7079
#define CK_ScreenNext                   7080
#define CK_ScreenPrev                   7081
#define CK_FilePrev                     111
#define CK_FileNext                     112
#define CK_DeleteToWordBegin            24
#define CK_DeleteToWordEnd              25
#define CK_ShowNumbers                  490
#define CK_Store                        701
#define CK_Cut                          702
#define CK_Paste                        703
#define CK_Mark                         201
#define CK_MarkLeft                     606
#define CK_MarkRight                    607
#define CK_MarkUp                       8021
#define CK_MarkDown                     8022
#define CK_MarkToWordBegin              608
#define CK_MarkToWordEnd                609
#define CK_MarkToHome                   612
#define CK_MarkToEnd                    613
#define CK_ToggleNavigation             5027
#define CK_Sort                         412
#define CK_Options                      7007
#define CK_LearnKeys                    7037
#define CK_Bookmark                     5013
#define CK_Quit                         402
#define CK_QuitQuiet                    7050
/* C-x or similar */
#define CK_ExtendedKeyMap               820

/* main commands */
#ifdef USE_INTERNAL_EDIT
#define CK_EditForceInternal            7082
#endif
#define CK_View                         7071
#define CK_ViewRaw                      8011
#define CK_ViewFile                     7072
#define CK_ViewFiltered                 7029
#define CK_Find                         419
#define CK_DirSize                      7058
#define CK_HotListAdd                   7001
#define CK_PanelListingChange           7002
#define CK_CompareDirs                  7006
#define CK_OptionsVfs                   7008
#define CK_OptionsConfirm               7009
#define CK_PutCurrentLink               7012
#define CK_PutOtherLink                 7015
#define CK_OptionsDisplayBits           7019
#define CK_EditExtensionsFile           7021
#define CK_EditFileHighlightFile        7022
#define CK_LinkSymbolicEdit             7024
#define CK_ExternalPanelize             7027
#define CK_Filter                       7028
#define CK_ConnectFish                  7031
#define CK_ConnectFtp                   7032
#define CK_ConnectSmb                   7059
#define CK_PanelInfo                    7034
#define CK_Jobs                         7035
#define CK_OptionsLayout                7036
#define CK_Link                         7038
#define CK_PanelListing                 7039
#define CK_ListMode                     7042
#define CK_CdQuick                      7047
#define CK_PanelQuickView               7049
#define CK_VfsList                      7054
#define CK_SaveSetup                    7056
#define CK_LinkSymbolic                 7062
#define CK_PanelListingSwitch           7063
#define CK_ShowHidden                   7064
#define CK_PanelTree                    7065
#define CK_Tree                         7066
#define CK_Undelete                     7067
#define CK_SplitVertHoriz               7075
#ifdef USE_DIFF_VIEW
#define CK_CompareFiles                 7076
#endif /* USE_DIFF_VIEW */
#define CK_OptionsPanel                 7077
#define CK_LinkSymbolicRelative         7078
#define CK_PutCurrentPath               7011
#define CK_PutOtherPath                 7014
#define CK_PutCurrentTagged             7013
#define CK_PutOtherTagged               7016
#define CK_Select                       7057
#define CK_Unselect                     7068
#define CK_SelectInvert                 7055

/* panels */
#define CK_PanelOtherCd                 8001
#define CK_PanelOtherCdLink             8002
#define CK_CopySingle                   8003
#define CK_MoveSingle                   8007
#define CK_DeleteSingle                 8004
#define CK_CdChild                      8012
#define CK_CdParent                     8013
#define CK_CdParentSmart                8043
#define CK_PanelOtherSync               8034
#define CK_SortNext                     8035
#define CK_SortPrev                     8036
#define CK_SortReverse                  8038
#define CK_SortByName                   8039
#define CK_SortByExt                    8040
#define CK_SortBySize                   8041
#define CK_SortByMTime                  8042

/* dialog */
#define CK_Ok                           3001
#define CK_Cancel                       3002

/* input */
#define CK_Yank                         4018

/* help */
#define CK_Index                        1002
#define CK_Back                         1003
#define CK_LinkNext                     1008
#define CK_LinkPrev                     1009
#define CK_NodeNext                     1010
#define CK_NodePrev                     1011

/* tree */
#define CK_Forget                       6003

#ifdef USE_INTERNAL_EDIT
/* cursor movements */
#define CK_Tab               14
#define CK_Undo              15
#define CK_ScrollUp          19
#define CK_ScrollDown        20
#define CK_Return            21
#define CK_ParagraphUp       26
#define CK_ParagraphDown     27
/* file commands */
#define CK_Save             101
#define CK_EditFile         102
#define CK_EditNew          103
#define CK_EditSyntaxFile   121
/* block commands */
#define CK_Unmark            206
#define CK_BlockSave         207
#define CK_MarkColumn        208
#define CK_BlockShiftLeft    211
#define CK_BlockShiftRight   212
#define CK_MarkAll           213
#define CK_MarkWord          214
#define CK_MarkLine          215
#define CK_InsertFile           401
#define CK_InsertOverwrite      403
#define CK_Date                 405
#define CK_DeleteLine           408
#define CK_DeleteToHome         409
#define CK_DeleteToEnd          410
#define CK_Mail                 413
#define CK_ParagraphFormat      416
#define CK_MatchBracket         421
#define CK_ExternalCommand      424
#define CK_OptionsSaveMode      428
#define CK_About                430
#define CK_ShowMargin           460
#define CK_ShowTabTws           470
#define CK_SyntaxOnOff          480
#define CK_SyntaxChoose         429
#define CK_InsertLiteral        851
/* bookmarks */
#define CK_BookmarkFlush  551
#define CK_BookmarkNext   552
#define CK_BookmarkPrev   553
/* mark commands */
#define CK_MarkPageUp                  604
#define CK_MarkPageDown                605
#define CK_MarkToFileBegin             614
#define CK_MarkToFileEnd               615
#define CK_MarkToPageBegin             616
#define CK_MarkToPageEnd               617
#define CK_MarkScrollUp                618
#define CK_MarkScrollDown              619
#define CK_MarkParagraphUp             620
#define CK_MarkParagraphDown           621
/* column mark commands */
#define CK_MarkColumnPageUp                654
#define CK_MarkColumnPageDown              655
#define CK_MarkColumnLeft                  656
#define CK_MarkColumnRight                 657
#define CK_MarkColumnUp                    660
#define CK_MarkColumnDown                  661
#define CK_MarkColumnScrollUp              668
#define CK_MarkColumnScrollDown            669
#define CK_MarkColumnParagraphUp           670
#define CK_MarkColumnParagraphDown         671
/* macros */
#define CK_MacroStartRecord     501
#define CK_MacroStopRecord      502
#define CK_MacroStartStopRecord 853
#define CK_MacroExecute         852
#define CK_MacroDelete          503
#define CK_RepeatStartStopRecord         854
#define CK_RepeatStartRecord             855
#define CK_RepeatStopRecord              856
#endif /* USE_INTERNAL_EDIT */

/* viewer */
#define CK_WrapMode                     5002
#define CK_HexEditMode                  5003
#define CK_HexMode                      5004
#define CK_MagicMode                    5008
#define CK_NroffMode                    5009
#define CK_BookmarkGoto                 5012
#define CK_Ruler                        5026

#ifdef USE_DIFF_VIEW
/* diff viewer */
#define CK_ShowSymbols                  9001
#define CK_SplitFull                    9003
#define CK_SplitEqual                   9004
#define CK_SplitMore                    9005
#define CK_SplitLess                    9006
#define CK_Tab2                         9009
#define CK_Tab3                         9010
#define CK_Tab4                         9011
#define CK_Tab8                         9012
#define CK_HunkNext                     9015
#define CK_HunkPrev                     9016
#define CK_EditOther                    9019
#define CK_Merge                        9035
#endif /* USE_DIFF_VIEW */

#define CK_PipeBlock(i)  (10000+(i))
#define CK_Macro(i)      (20000+(i))
#define CK_Last_Macro    CK_Macro(0x7FFF)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct name_keymap_t
{
    const char *name;
    unsigned long val;
} name_keymap_t;

typedef struct key_config_t
{
    time_t mtime;               /* mtime at the moment we read config file */
    GArray *keymap;
    GArray *ext_keymap;
    gchar *labels[10];
} key_config_t;

/* The global keymaps are of this type */
typedef struct global_keymap_t
{
    long key;
    unsigned long command;
    char caption[KEYMAP_SHORTCUT_LENGTH];
} global_keymap_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void keybind_cmd_bind (GArray * keymap, const char *keybind, unsigned long action);
unsigned long keybind_lookup_action (const char *name);
const char *keybind_lookup_actionname (unsigned long action);
const char *keybind_lookup_keymap_shortcut (const global_keymap_t * keymap, unsigned long action);
unsigned long keybind_lookup_keymap_command (const global_keymap_t * keymap, long key);

/*** inline functions ****************************************************************************/

#endif /* MC__KEYBIND_H */
