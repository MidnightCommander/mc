#ifndef MC_EDIT_CMD_DEF_H
#define MC_EDIT_CMD_DEF_H

/*  in the distant future, keyboards will be invented with a
    separate key for each one of these commands   *sigh*      */

/* special commands */
#define CK_Insert_Char		-1
#define CK_Ignore_Key		0

/* cursor movements */
#define CK_BackSpace		1
#define CK_Delete		2
#define CK_Enter		3
#define CK_Page_Up		4
#define CK_Page_Down		5
#define CK_Left			6
#define CK_Right		7
#define CK_Word_Left		8
#define CK_Word_Right		9
#define CK_Up			10
#define CK_Down			11
#define CK_Home			12
#define CK_End			13
#define CK_Tab			14
#define CK_Undo			15
#define CK_Beginning_Of_Text	16
#define CK_End_Of_Text		17
#define CK_Scroll_Up		18
#define CK_Scroll_Down		19
#define CK_Return		20
#define CK_Begin_Page		21
#define CK_End_Page		22
#define CK_Delete_Word_Left	23
#define CK_Delete_Word_Right	24
#define CK_Paragraph_Up		25
#define CK_Paragraph_Down	26


/* file commands */
#define CK_Save			101
#define CK_Load			102
#define CK_New			103
#define CK_Save_As		104

/* block commands */
#define CK_Mark			201
#define CK_Copy			202
#define CK_Move			203
#define CK_Remove		204
#define CK_Unmark		206
#define CK_Save_Block		207
#define CK_Column_Mark		208

/* search and replace */
#define CK_Find			301
#define CK_Find_Again		302
#define CK_Replace		303
#define CK_Replace_Again	304
#define CK_Complete_Word	305

/* debugger commands */
#define CK_Debug_Start		350
#define CK_Debug_Stop		351
#define CK_Debug_Toggle_Break	352
#define CK_Debug_Clear		353
#define CK_Debug_Next		354
#define CK_Debug_Step		355
#define CK_Debug_Back_Trace	356
#define CK_Debug_Continue	357
#define CK_Debug_Enter_Command	358
#define CK_Debug_Until_Curser	359

/* misc */
#define CK_Insert_File		401
#define CK_Exit			402
#define CK_Toggle_Insert        403
#define CK_Help			404
#define CK_Date			405
#define CK_Refresh		406
#define CK_Goto			407
#define CK_Delete_Line		408
#define CK_Delete_To_Line_End	409
#define CK_Delete_To_Line_Begin	410
#define CK_Man_Page		411
#define CK_Sort			412
#define CK_Mail			413
#define CK_Cancel		414
#define CK_Complete		415
#define CK_Paragraph_Format	416
#define CK_Util			417
#define CK_Type_Load_Python	418
#define CK_Find_File		419
#define CK_Ctags		420
#define CK_Match_Bracket	421
#define CK_Terminal		422
#define CK_Terminal_App		423
#define CK_ExtCmd		424

#define CK_User_Menu		425
/* application control */
#define CK_Save_Desktop		451
#define CK_New_Window		452
#define CK_Cycle		453
#define CK_Menu			454
#define CK_Save_And_Quit	455
#define CK_Run_Another		456
#define CK_Check_Save_And_Quit	457
#define CK_Maximize		458

/* macro */
#define CK_Begin_Record_Macro	501
#define CK_End_Record_Macro	502
#define CK_Delete_Macro		503

/* book mark */
#define CK_Toggle_Bookmark	550
#define CK_Flush_Bookmarks	551
#define CK_Next_Bookmark	552
#define CK_Prev_Bookmark	553

/* highlight commands */
#define CK_Page_Up_Highlight		604
#define CK_Page_Down_Highlight		605
#define CK_Left_Highlight		606
#define CK_Right_Highlight		607
#define CK_Word_Left_Highlight		608
#define CK_Word_Right_Highlight		609
#define CK_Up_Highlight			610
#define CK_Down_Highlight		611
#define CK_Home_Highlight		612
#define CK_End_Highlight		613
#define CK_Beginning_Of_Text_Highlight	614
#define CK_End_Of_Text_Highlight	615
#define CK_Begin_Page_Highlight		616
#define CK_End_Page_Highlight		617
#define CK_Scroll_Up_Highlight		618
#define CK_Scroll_Down_Highlight	619
#define CK_Paragraph_Up_Highlight	620
#define CK_Paragraph_Down_Highlight	621


/* X clipboard operations */
#define CK_XStore		701
#define CK_XCut			702
#define CK_XPaste		703
#define CK_Selection_History	704

#define CK_Shell		801

/*
   Process a block through a shell command: CK_Pipe_Block(i) executes shell_cmd[i].
   shell_cmd[i] must process the file ~/cooledit.block and output ~/cooledit.block
   which is then inserted into the text in place of the original block. shell_cmd[i] must
   also produce a file homedir/cooledit.error . If this file is not empty an error will
   have been assumed to have occured, and the block will not be replaced.
   TODO: bring up a viewer to display the error message instead of inserting
   it into the text, which is annoying.
 */
#define CK_Pipe_Block(i)	(1000+(i))
#define SHELL_COMMANDS_i {"/edit.indent.rc", "/edit.spell.rc", /* and so on */ 0 }
#define CK_Macro(i)		(2000+(i))
#define CK_Last_Macro		CK_Macro(0x7FFF)
#else

#define CK_User_Command(i)	((i) | (1 << 16))
#define IS_USER_COMMAND(i)	((i) & (1 << 16))
#define CK_Macro(i)		((i) | (1 << 17))
#define IS_MACRO_COMMAND(i)	((i) & (1 << 17))

#endif
