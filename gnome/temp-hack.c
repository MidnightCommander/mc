#include "global.h"
#include "color.h"
static struct stat *s_stat, *d_stat;

/* Used for button result values */
enum {
    REPLACE_YES = B_USER,
    REPLACE_NO,
    REPLACE_APPEND,
    REPLACE_ALWAYS,
    REPLACE_UPDATE,
    REPLACE_NEVER,
    REPLACE_ABORT,
    REPLACE_SIZE,
    REPLACE_REGET
} FileReplaceCode;

static int
replace_callback (struct Dlg_head *h, int Id, int Msg)
{
#ifndef HAVE_X

    switch (Msg){
    case DLG_DRAW:
	dialog_repaint (h, ERROR_COLOR, ERROR_COLOR);
	break;
    }
#endif
    return 0;
}

#ifdef HAVE_X
#define X_TRUNC 128
#else
#define X_TRUNC 52
#endif

/* File operate window sizes */
#define WX 62
#define WY 10
#define BY 10
#define WX_ETA_EXTRA  12

static int       replace_colors [4];
static Dlg_head *replace_dlg;
/*
 * FIXME: probably it is better to replace this with quick dialog machinery,
 * but actually I'm not familiar with it and have not much time :(
 *   alex
 */
static struct
{
	char* text;
	int   ypos, xpos;	
	int   value;		/* 0 for labels */
	char* tkname;
	WLay  layout;
}
rd_widgets [] =
{
	{N_("Target file \"%s\" already exists!"),
	                 3,      4,  0,              "target-e",   XV_WLAY_CENTERROW},
	{N_("&Abort"),   BY + 3, 25, REPLACE_ABORT,  "abort",      XV_WLAY_CENTERROW},
	{N_("if &Size differs"),
	                 BY + 1, 28, REPLACE_SIZE,   "if-size",    XV_WLAY_RIGHTOF},
	{N_("non&E"),    BY,     47, REPLACE_NEVER,  "none",       XV_WLAY_RIGHTOF},
	{N_("&Update"),  BY,     36, REPLACE_UPDATE, "update",     XV_WLAY_RIGHTOF},
	{N_("al&L"),     BY,     28, REPLACE_ALWAYS, "all",        XV_WLAY_RIGHTOF},
	{N_("Overwrite all targets?"),
	                 BY,     4,  0,              "over-label", XV_WLAY_CENTERROW},
	{N_("&Reget"),   BY - 1, 28, REPLACE_REGET,  "reget",      XV_WLAY_RIGHTOF},
	{N_("ap&Pend"),  BY - 2, 45, REPLACE_APPEND, "append",     XV_WLAY_RIGHTOF},
	{N_("&No"),      BY - 2, 37, REPLACE_NO,     "no",         XV_WLAY_RIGHTOF},
	{N_("&Yes"),     BY - 2, 28, REPLACE_YES,    "yes",        XV_WLAY_RIGHTOF},
	{N_("Overwrite this target?"),
	                 BY - 2, 4,  0,              "overlab",    XV_WLAY_CENTERROW},
	{N_("Target date: %s, size %d"),
	                 6,      4,  0,              "target-date",XV_WLAY_CENTERROW},
	{N_("Source date: %s, size %d"),
	                 5,      4,  0,              "source-date",XV_WLAY_CENTERROW}
}; 

#define ADD_RD_BUTTON(i)\
	add_widgetl (replace_dlg,\
		button_new (rd_widgets [i].ypos, rd_widgets [i].xpos, rd_widgets [i].value,\
		NORMAL_BUTTON, rd_widgets [i].text, 0, 0, rd_widgets [i].tkname), \
		rd_widgets [i].layout)

#define ADD_RD_LABEL(i,p1,p2)\
	sprintf (buffer, rd_widgets [i].text, p1, p2);\
	add_widgetl (replace_dlg,\
		label_new (rd_widgets [i].ypos, rd_widgets [i].xpos, buffer, rd_widgets [i].tkname),\
		rd_widgets [i].layout)

static void
init_replace (enum OperationMode mode)
{
    char buffer [128];
	static int rd_xlen = 60, rd_trunc = X_TRUNC;

#ifdef ENABLE_NLS
	static int i18n_flag;
	if (!i18n_flag)
	{
		int l1, l2, l, row;
		register int i = sizeof (rd_widgets) / sizeof (rd_widgets [0]); 
		while (i--)
			rd_widgets [i].text = _(rd_widgets [i].text);

		/* 
		 *longest of "Overwrite..." labels 
		 * (assume "Target date..." are short enough)
		 */
		l1 = max (strlen (rd_widgets [6].text), strlen (rd_widgets [11].text));

		/* longest of button rows */
		i = sizeof (rd_widgets) / sizeof (rd_widgets [0]);
		for (row = l = l2 = 0; i--;)
		{
			if (rd_widgets [i].value != 0)
			{
				if (row != rd_widgets [i].ypos)
				{
					row = rd_widgets [i].ypos;
					l2 = max (l2, l);
					l = 0;
				}
				l += strlen (rd_widgets [i].text) + 4;
			}
		}
		l2 = max (l2, l); /* last row */
		rd_xlen = max (rd_xlen, l1 + l2 + 8);
		rd_trunc = rd_xlen - 6;

		/* Now place buttons */
		l1 += 5; /* start of first button in the row */
		i = sizeof (rd_widgets) / sizeof (rd_widgets [0]);
		
		for (l = l1, row = 0; --i > 1;)
		{
			if (rd_widgets [i].value != 0)
			{
				if (row != rd_widgets [i].ypos)
				{
					row = rd_widgets [i].ypos;
					l = l1;
				}
				rd_widgets [i].xpos = l;
				l += strlen (rd_widgets [i].text) + 4;
			}
		}
		/* Abort button is centered */
		rd_widgets [1].xpos = (rd_xlen - strlen (rd_widgets [1].text) - 3) / 2;

	}
#endif /* ENABLE_NLS */

    replace_colors [0] = ERROR_COLOR;
    replace_colors [1] = COLOR_NORMAL;
    replace_colors [2] = ERROR_COLOR;
    replace_colors [3] = COLOR_NORMAL;
    
    replace_dlg = create_dlg (0, 0, 16, rd_xlen, replace_colors, replace_callback,
			      "[ Replace ]", "replace", DLG_CENTER);
    
    x_set_dialog_title (replace_dlg,
        mode == Foreground ? _(" File exists ") : _(" Background process: File exists "));


	ADD_RD_LABEL(0, name_trunc (file_progress_replace_filename, rd_trunc - strlen (rd_widgets [0].text)), 0 );
	ADD_RD_BUTTON(1);    
    
	ADD_RD_BUTTON(2);
	ADD_RD_BUTTON(3);
	ADD_RD_BUTTON(4);
	ADD_RD_BUTTON(5);
	ADD_RD_LABEL(6,0,0);

    /* "this target..." widgets */
	if (!S_ISDIR (d_stat->st_mode)){
		if ((d_stat->st_size && s_stat->st_size > d_stat->st_size))
			ADD_RD_BUTTON(7);

		ADD_RD_BUTTON(8);
    }
	ADD_RD_BUTTON(9);
	ADD_RD_BUTTON(10);
	ADD_RD_LABEL(11,0,0);
    
	ADD_RD_LABEL(12, file_date (d_stat->st_mtime), (int) d_stat->st_size);
	ADD_RD_LABEL(13, file_date (s_stat->st_mtime), (int) s_stat->st_size);
}

FileProgressStatus
file_progress_real_query_replace (enum OperationMode mode, char *destname, struct stat *_s_stat,
				  struct stat *_d_stat)
{
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");
    g_warning ("memo: file_progress_real_query_replace!\n");

    /* Better to have something than nothing at all */
    if (file_progress_replace_result < REPLACE_ALWAYS){
	file_progress_replace_filename = destname;
	s_stat = _s_stat;
	d_stat = _d_stat;
	init_replace (mode);
	run_dlg (replace_dlg);
	file_progress_replace_result = replace_dlg->ret_value;
	if (file_progress_replace_result == B_CANCEL)
	    file_progress_replace_result = REPLACE_ABORT;
	destroy_dlg (replace_dlg);
    }

    switch (file_progress_replace_result){
    case REPLACE_UPDATE:
	do_refresh ();
	if (_s_stat->st_mtime > _d_stat->st_mtime)
	    return FILE_CONT;
	else
	    return FILE_SKIP;

    case REPLACE_SIZE:
	do_refresh ();
	if (_s_stat->st_size == _d_stat->st_size)
	    return FILE_SKIP;
	else
	    return FILE_CONT;
	
    case REPLACE_REGET:
	/* Carefull: we fall through and set do_append */
	file_progress_do_reget = _d_stat->st_size;
	
    case REPLACE_APPEND:
        file_progress_do_append = 1;
	
    case REPLACE_YES:
    case REPLACE_ALWAYS:
	do_refresh ();
	return FILE_CONT;
    case REPLACE_NO:
    case REPLACE_NEVER:
	do_refresh ();
	return FILE_SKIP;
    case REPLACE_ABORT:
    default:
	return FILE_ABORT;
    }
}


