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

/* cursor movements */
#define CK_BackSpace         1
#define CK_Delete            2
#define CK_Enter             3
#define CK_Page_Up           4
#define CK_Page_Down         5
#define CK_Left              6
#define CK_Right             7
#define CK_Word_Left         8
#define CK_Word_Right        9
#define CK_Up                10
#define CK_Down              11
#define CK_Home              12
#define CK_End               13
#define CK_Tab               14
#define CK_Undo              15
#define CK_Beginning_Of_Text 16
#define CK_End_Of_Text       17
#define CK_Scroll_Up         18
#define CK_Scroll_Down       19
#define CK_Return            20
#define CK_Begin_Page        21
#define CK_End_Page          22
#define CK_Delete_Word_Left  23
#define CK_Delete_Word_Right 24
#define CK_Paragraph_Up      25
#define CK_Paragraph_Down    26

/* file commands */
#define CK_Save             101
#define CK_Load             102
#define CK_New              103
#define CK_Save_As          104
#define CK_Load_Prev_File   111
#define CK_Load_Next_File   112
#define CK_Load_Syntax_File 121
#define CK_Load_Menu_File   122
#define CK_Menu             123

/* block commands */
#define CK_Mark              201
#define CK_Copy              202
#define CK_Move              203
#define CK_Remove            204
#define CK_Unmark            206
#define CK_Save_Block        207
#define CK_Column_Mark       208
#define CK_Shift_Block_Left  211
#define CK_Shift_Block_Right 212
#define CK_Mark_All          213
#define CK_Mark_Word         214
#define CK_Mark_Line         215

/* search and replace */
#define CK_Find          301
#define CK_Find_Again    302
#define CK_Replace       303
#define CK_Replace_Again 304
#define CK_Complete_Word 305

#if 0
/* debugger commands */
#define CK_Debug_Start         350
#define CK_Debug_Stop          351
#define CK_Debug_Toggle_Break  352
#define CK_Debug_Clear         353
#define CK_Debug_Next          354
#define CK_Debug_Step          355
#define CK_Debug_Back_Trace    356
#define CK_Debug_Continue      357
#define CK_Debug_Enter_Command 358
#define CK_Debug_Until_Curser  359
#endif

/* misc */
#define CK_Insert_File          401
#define CK_Quit                 402
#define CK_Toggle_Insert        403
#define CK_Help                 404
#define CK_Date                 405
#define CK_Refresh              406
#define CK_Goto                 407
#define CK_Delete_Line          408
#define CK_Delete_To_Line_End   409
#define CK_Delete_To_Line_Begin 410
#define CK_Man_Page             411
#define CK_Sort                 412
#define CK_Mail                 413
#define CK_Cancel               414
#define CK_Complete             415
#define CK_Paragraph_Format     416
#define CK_Util                 417
#define CK_Type_Load_Python     418
#define CK_Find_File            419
#define CK_Ctags                420
#define CK_Match_Bracket        421
#define CK_Terminal             422
#define CK_Terminal_App         423
#define CK_ExtCmd               424
#define CK_User_Menu            425
#define CK_Find_Definition      426
#define CK_Edit_Options         427
#define CK_Edit_Save_Mode       428
#define CK_Choose_Syntax        429
#define CK_About                430

#if 0
/* application control */
#define CK_Save_Desktop        451
#define CK_New_Window          452
#define CK_Cycle               453
#define CK_Save_And_Quit       455
#define CK_Run_Another         456
#define CK_Check_Save_And_Quit 457
#define CK_Maximize            458
#endif

#define CK_Toggle_Show_Margin 460
#define CK_Toggle_Tab_TWS     470
#define CK_Toggle_Syntax      480
#define CK_Toggle_Line_State  490

/* macro */
#define CK_Begin_Record_Macro 501
#define CK_End_Record_Macro   502
#define CK_Delete_Macro       503

/* book mark */
#define CK_Toggle_Bookmark 550
#define CK_Flush_Bookmarks 551
#define CK_Next_Bookmark   552
#define CK_Prev_Bookmark   553

/* highlight commands */
#define CK_Page_Up_Highlight           604
#define CK_Page_Down_Highlight         605
#define CK_Left_Highlight              606
#define CK_Right_Highlight             607
#define CK_Word_Left_Highlight         608
#define CK_Word_Right_Highlight        609
#define CK_Up_Highlight                610
#define CK_Down_Highlight              611
#define CK_Home_Highlight              612
#define CK_End_Highlight               613
#define CK_Beginning_Of_Text_Highlight 614
#define CK_End_Of_Text_Highlight       615
#define CK_Begin_Page_Highlight        616
#define CK_End_Page_Highlight          617
#define CK_Scroll_Up_Highlight         618
#define CK_Scroll_Down_Highlight       619
#define CK_Paragraph_Up_Highlight      620
#define CK_Paragraph_Down_Highlight    621

/* alt highlight commands */
#define CK_Page_Up_Alt_Highlight           654
#define CK_Page_Down_Alt_Highlight         655
#define CK_Left_Alt_Highlight              656
#define CK_Right_Alt_Highlight             657
#define CK_Word_Left_Alt_Highlight         658
#define CK_Word_Right_Alt_Highlight        659
#define CK_Up_Alt_Highlight                660
#define CK_Down_Alt_Highlight              661
#define CK_Home_Alt_Highlight              662
#define CK_End_Alt_Highlight               663
#define CK_Beginning_Of_Text_Alt_Highlight 664
#define CK_End_Of_Text_Alt_Highlight       665
#define CK_Begin_Page_Alt_Highlight        666
#define CK_End_Page_Alt_Highlight          667
#define CK_Scroll_Up_Alt_Highlight         668
#define CK_Scroll_Down_Alt_Highlight       669
#define CK_Paragraph_Up_Alt_Highlight      670
#define CK_Paragraph_Down_Alt_Highlight    671

/* X clipboard operations */
#define CK_XStore            701
#define CK_XCut              702
#define CK_XPaste            703
#define CK_Selection_History 704

#define CK_Shell           801

/* C-x or similar */
#define CK_Ext_Mode        820

#define CK_Insert_Literal  851
#define CK_Execute_Macro   852
#define CK_Begin_End_Macro 853

/* help */
#define CK_HelpHelp                     1001
#define CK_HelpIndex                    1002
#define CK_HelpBack                     1003
#define CK_HelpQuit                     1004
#define CK_HelpMoveUp                   1005
#define CK_HelpMoveDown                 1006
#define CK_HelpSelectLink               1007
#define CK_HelpNextLink                 1008
#define CK_HelpPrevLink                 1009
#define CK_HelpNextNode                 1010
#define CK_HelpPrevNode                 1011
#define CK_HelpMovePgDn                 1012
#define CK_HelpMovePgUp                 1013
#define CK_HelpMoveHalfPgDn             1014
#define CK_HelpMoveHalfPgUp             1015
#define CK_HelpMoveTop                  1016
#define CK_HelpMoveBottom               1017

/* common */
#define CK_ShowCommandLine              2001
#define CK_SelectCodepage               2002
#define CK_StartExtMap1                 2021
#define CK_StartExtMap2                 2022

/* dialog */
#define CK_DialogOK                    3001
#define CK_DialogCancel                3002
#define CK_DialogPrevItem              3003
#define CK_DialogNextItem              3004
#define CK_DialogHelp                  3005
#define CK_DialogSuspend               3006
#define CK_DialogRefresh               3007

/* text fields */
#define CK_InputBol                    4001
#define CK_InputEol                    4002
#define CK_InputMoveLeft               4003
#define CK_InputWordLeft               4004
#define CK_InputBackwardChar           4005
#define CK_InputBackwardWord           4006
#define CK_InputMoveRight              4007
#define CK_InputWordRight              4008
#define CK_InputForwardChar            4009
#define CK_InputForwardWord            4010
#define CK_InputBackwardDelete         4011
#define CK_InputDeleteChar             4012
#define CK_InputKillWord               4013
#define CK_InputBackwardKillWord       4014
#define CK_InputSetMark                4015
#define CK_InputKillRegion             4016
#define CK_InputKillSave               4017
#define CK_InputYank                   4018
#define CK_InputCopyRegion             4019
#define CK_InputKillLine               4020
#define CK_InputHistoryPrev            4021
#define CK_InputHistoryNext            4022
#define CK_InputHistoryShow            4023
#define CK_InputComplete               4024
#define CK_InputPaste                  4025
#define CK_InputClearLine              4026
#define CK_InputLeftHighlight          4027
#define CK_InputRightHighlight         4028
#define CK_InputWordLeftHighlight      4029
#define CK_InputWordRightHighlight     4030
#define CK_InputBolHighlight           4031
#define CK_InputEolHighlight           4032

/* listbox */
#define CK_ListboxMoveUp               4500
#define CK_ListboxMoveDown             4501
#define CK_ListboxMoveHome             4502
#define CK_ListboxMoveEnd              4503
#define CK_ListboxMovePgUp             4504
#define CK_ListboxMovePgDn             4505
#define CK_ListboxDeleteItem           4506
#define CK_ListboxDeleteAll            4507

/* viewer */
#define CK_ViewHelp                     5001
#define CK_ViewToggleWrapMode           5002
#define CK_ViewToggleHexEditMode        5003
#define CK_ViewToggleHexMode            5004
#define CK_ViewGoto                     5005
#define CK_ViewHexEditSave              5006
#define CK_ViewSearch                   5007
#define CK_ViewToggleMagicMode          5008
#define CK_ViewToggleNroffMode          5009
#define CK_ViewQuit                     5010
#define CK_ViewContinueSearch           5011
#define CK_ViewGotoBookmark             5012
#define CK_ViewNewBookmark              5013
#define CK_ViewMoveUp                   5014
#define CK_ViewMoveDown                 5015
#define CK_ViewMoveLeft                 5016
#define CK_ViewMoveRight                5017
#define CK_ViewMovePgDn                 5018
#define CK_ViewMovePgUp                 5019
#define CK_ViewMoveHalfPgDn             5020
#define CK_ViewMoveHalfPgUp             5021
#define CK_ViewMoveToBol                5022
#define CK_ViewMoveToEol                5023
#define CK_ViewNextFile                 5024
#define CK_ViewPrevFile                 5025
#define CK_ViewToggleRuler              5026
#define CK_ViewToggleHexNavMode         5027
#define CK_ViewMoveTop                  5028
#define CK_ViewMoveBottom               5029
#define CK_ViewMoveLeft10               5030
#define CK_ViewMoveRight10              5031

/* tree */
#define CK_TreeHelp                     6001
#define CK_TreeForget                   6003
#define CK_TreeToggleNav                6004
#define CK_TreeCopy                     6005
#define CK_TreeMove                     6006
#define CK_TreeMake                     6007
#define CK_TreeMoveUp                   6011
#define CK_TreeMoveDown                 6012
#define CK_TreeMoveLeft                 6013
#define CK_TreeMoveRight                6014
#define CK_TreeMoveHome                 6015
#define CK_TreeMoveEnd                  6016
#define CK_TreeMovePgUp                 6017
#define CK_TreeMovePgDn                 6018
#define CK_TreeOpen                     6019
#define CK_TreeRescan                   6020
#define CK_TreeStartSearch              6021
#define CK_TreeRemove                   6022

/* main commands */
#define CK_AddHotlist                   7001
#define CK_ChangeListingCmd             7002
#define CK_ChmodCmd                     7003
#define CK_ChownAdvancedCmd             7004
#define CK_ChownCmd                     7005
#define CK_CompareDirsCmd               7006
#define CK_ConfigureBox                 7007
#define CK_ConfigureVfs                 7008
#define CK_ConfirmBox                   7009
#define CK_CopyCmd                      7010
#define CK_CopyCurrentPathname          7011
#define CK_CopyCurrentReadlink          7012
#define CK_CopyCurrentTagged            7013
#define CK_CopyOtherPathname            7014
#define CK_CopyOtherReadlink            7015
#define CK_CopyOtherTagged              7016
#define CK_DeleteCmd                    7017
#define CK_DirsizesCmd                  7018
#define CK_DisplayBitsBox               7019
#define CK_EditCmd                      7020
#define CK_EditExtFileCmd               7021
#define CK_EditFhlFileCmd               7022
#define CK_EditMcMenuCmd                7023
#define CK_EditSymlinkCmd               7024
#define CK_EditSyntaxCmd                7025
#define CK_EditUserMenuCmd              7026
#define CK_ExternalPanelize             7027
#define CK_FilterCmd                    7028
#define CK_FilteredViewCmd              7029
#define CK_FindCmd                      7030
#define CK_FishlinkCmd                  7031
#define CK_FtplinkCmd                   7032
#define CK_HistoryCmd                   7033
#define CK_InfoCmd                      7034
#define CK_JobsCmd                      7035
#define CK_LayoutBox                    7036
#define CK_LearnKeys                    7037
#define CK_LinkCmd                      7038
#define CK_ListingCmd                   7039
#define CK_ListmodeCmd                  7042
#define CK_MenuLastSelectedCmd          7044
#define CK_MkdirCmd                     7045
#define CK_QuickCdCmd                   7047
#define CK_QuickChdirCmd                7048
#define CK_QuickViewCmd                 7049
#define CK_QuietQuitCmd                 7050
#define CK_QuitCmd                      7051
#define CK_RenameCmd                    7052
#define CK_RereadCmd                    7053
#define CK_ReselectVfs                  7054
#define CK_ReverseSelectionCmd          7055
#define CK_SaveSetupCmd                 7056
#define CK_SelectCmd                    7057
#define CK_SingleDirsizeCmd             7058
#define CK_SmblinkCmd                   7059
#define CK_SuspendCmd                   7060
#define CK_SwapCmd                      7061
#define CK_SymlinkCmd                   7062
#define CK_ToggleListingCmd             7063
#define CK_ToggleShowHidden             7064
#define CK_TreeCmd                      7065
#define CK_TreeBoxCmd                   7066
#define CK_UndeleteCmd                  7067
#define CK_UnselectCmd                  7068
#define CK_UserFileMenuCmd              7069
#define CK_UserMenuCmd                  7070
#define CK_ViewCmd                      7071
#define CK_ViewFileCmd                  7072
#define CK_HelpCmd                      7073
#define CK_MenuCmd                      7074
#define CK_TogglePanelsSplit            7075
#define CK_DiffViewCmd                  7076
#define CK_PanelOptionsBox              7077
#define CK_RelativeSymlinkCmd           7078
#define CK_DialogListCmd                7079
#define CK_DialogNextCmd                7080
#define CK_DialogPrevCmd                7081
#define CK_EditForceInternalCmd         7082

/* panels */
#define CK_PanelChdirOtherPanel         8001
#define CK_PanelChdirToReadlink         8002
#define CK_PanelCmdCopyLocal            8003
#define CK_PanelCmdDeleteLocal          8004
#define CK_PanelCmdDoEnter              8005
#define CK_PanelCmdEditNew              8006
#define CK_PanelCmdRenameLocal          8007
#define CK_PanelCmdReverseSelection     8008
#define CK_PanelCmdSelect               8009
#define CK_PanelCmdUnselect             8010
#define CK_PanelCmdViewSimple           8011
#define CK_PanelGotoChildDir            8012
#define CK_PanelGotoParentDir           8013
#define CK_PanelDirectoryHistoryList    8014
#define CK_PanelDirectoryHistoryNext    8015
#define CK_PanelDirectoryHistoryPrev    8016
#define CK_PanelGotoBottomFile          8017
#define CK_PanelGotoMiddleFile          8018
#define CK_PanelGotoTopFile             8019
#define CK_PanelMarkFile                8020
#define CK_PanelMarkFileUp              8021
#define CK_PanelMarkFileDown            8022
#define CK_PanelMoveDown                8023
#define CK_PanelMoveEnd                 8024
#define CK_PanelMoveHome                8025
#define CK_PanelMoveUp                  8026
#define CK_PanelMoveLeft                8027
#define CK_PanelMoveRight               8028
#define CK_PanelNextPage                8029
#define CK_PanelPrevPage                8030
#define CK_PanelSetPanelEncoding        8031
#define CK_PanelStartSearch             8032
#define CK_PanelStopSearch              8033
#define CK_PanelSyncOtherPanel          8034
#define CK_PanelToggleSortOrderNext     8035
#define CK_PanelToggleSortOrderPrev     8036
#define CK_PanelSelectSortOrder         8037
#define CK_PanelReverseSort             8038
#define CK_PanelSortOrderByName         8039
#define CK_PanelSortOrderByExt          8040
#define CK_PanelSortOrderBySize         8041
#define CK_PanelSortOrderByMTime        8042
#define CK_PanelSmartGotoParentDir      8043

/* diff viewer */
#define CK_DiffDisplaySymbols           9001
#define CK_DiffDisplayNumbers           9002
#define CK_DiffFull                     9003
#define CK_DiffEqual                    9004
#define CK_DiffSplitMore                9005
#define CK_DiffSplitLess                9006
#define CK_DiffShowDiff                 9008
#define CK_DiffSetTab2                  9009
#define CK_DiffSetTab3                  9010
#define CK_DiffSetTab4                  9011
#define CK_DiffSetTab8                  9012
#define CK_DiffSwapPanel                9013
#define CK_DiffRedo                     9014
#define CK_DiffNextHunk                 9015
#define CK_DiffPrevHunk                 9016
#define CK_DiffGoto                     9017
#define CK_DiffEditCurrent              9018
#define CK_DiffEditOther                9019
#define CK_DiffSearch                   9020
#define CK_DiffEOF                      9021
#define CK_DiffBOF                      9022
#define CK_DiffDown                     9023
#define CK_DiffUp                       9024
#define CK_DiffLeft                     9025
#define CK_DiffRight                    9026
#define CK_DiffQuickLeft                9027
#define CK_DiffQuickRight               9028
#define CK_DiffPageDown                 9029
#define CK_DiffPageUp                   9030
#define CK_DiffHome                     9031
#define CK_DiffEnd                      9032
#define CK_DiffQuit                     9033
#define CK_DiffHelp                     9034
#define CK_DiffMergeCurrentHunk         9035
#define CK_DiffSave                     9036
#define CK_DiffContinueSearch           9037
#define CK_DiffOptions                  9038

/*
   Process a block through a shell command: CK_Pipe_Block(i) executes shell_cmd[i].
   shell_cmd[i] must process the file ~/cooledit.block and output ~/cooledit.block
   which is then inserted into the text in place of the original block. shell_cmd[i] must
   also produce a file homedir/cooledit.error . If this file is not empty an error will
   have been assumed to have occured, and the block will not be replaced.
   TODO: bring up a viewer to display the error message instead of inserting
   it into the text, which is annoying.
 */
#define CK_Pipe_Block(i) (1000+(i))
#define SHELL_COMMANDS_i {"/edit.indent.rc", "/edit.spell.rc", /* and so on */ 0 }
#define CK_Macro(i)      (2000+(i))
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
const char *keybind_lookup_keymap_shortcut (const global_keymap_t * keymap, unsigned long action);
unsigned long keybind_lookup_keymap_command (const global_keymap_t * keymap, long key);

/*** inline functions ****************************************************************************/

#endif /* MC__KEYBIND_H */
