/*
 * GtkDTree: A directory tree view 
 *
 * Original version by Daniel Lacroix (LACROIX@wanadoo.fr)
 *
 * Adapted to the Midnight Commander by Miguel.
 *
 */
#include <config.h>
#include <gnome.h>
#include "gtree.h"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dir-open.xpm"
#include "dir-close.xpm"

#define TREE_SPACING 5

static GtkCTreeClass *parent_class = NULL;

enum {
	DIRECTORY_CHANGED,
	LAST_SIGNAL
};

static guint gtk_dtree_signals [LAST_SIGNAL] = { 0 };

static char *
gtk_dtree_get_row_path (GtkCTree *ctree, GtkCTreeNode *row, gint column)
{
	char *node_text, *path;

	path = g_strdup ("");
	do {
		char *new_path;
		int val;
		
		val = gtk_ctree_node_get_pixtext (
			ctree, row, column,
			&node_text, NULL, NULL, NULL);

		if (!val)
			return path;
		
		new_path = g_concat_dir_and_file (node_text, path);
		g_free (path);
		path = new_path;

		row = GTK_CTREE_ROW (row)->parent;
	} while (row);

	return path;
}

static gboolean
gtk_dtree_load_path (GtkDTree *dtree, char *path, GtkCTreeNode *parent, int level)
{
	DIR *dir;
	struct dirent *dirent;
	char *full_name;
	
	g_assert (path);
	g_assert (parent);
	g_assert (dtree);

	dir = opendir (path);
	if (!dir)
		return FALSE;

	for (; (dirent = readdir (dir)) != NULL; ){
		GtkCTreeNode *sibling;
		struct stat s;
		char *full_name;
		char *text [1];
		int   res;
		
		if (dirent->d_name [0] == '.'){
			if (dirent->d_name [1] == '.'){
				if (dirent->d_name [2] == 0)
					continue;
			} else if (dirent->d_name [1] == 0)
				continue;
		}

		full_name = g_concat_dir_and_file (path, dirent->d_name);
		res = stat (full_name, &s);

		if (res == -1){
			g_free (full_name);
			continue;
		}

		if (!S_ISDIR (s.st_mode)){
			g_free (full_name);
			continue;
		}

		text [0] = dirent->d_name;

		sibling = gtk_ctree_insert_node (
			GTK_CTREE (dtree), parent, NULL,
			text, TREE_SPACING,
			dtree->pixmap_close,
			dtree->bitmap_close,
			dtree->pixmap_open,
			dtree->bitmap_open,
			FALSE, FALSE);
		if (level)
			gtk_dtree_load_path (dtree, full_name, sibling, level-1);

		g_free (full_name);
	}

	closedir (dir);
}

static void
gtk_dtree_select_row (GtkCTree *ctree, GtkCTreeNode *row, gint column)
{
	GtkDTree *dtree = GTK_DTREE (ctree);
	GtkCTreeNode *last_node;
	char *path;

	parent_class->tree_select_row (ctree, row, column);

	while (row == dtree->root_node)
		return;

	if (row == dtree->last_node)
		return;

	dtree->last_node = row;

	gtk_clist_freeze (GTK_CLIST (ctree));
	path = gtk_dtree_get_row_path (ctree, row, 0);
	printf ("Selected: %s\n", path);
	
	if (dtree->current_path)
		g_free (dtree->current_path);

	dtree->current_path = path;

	gtk_signal_emit (GTK_OBJECT (ctree), gtk_dtree_signals [DIRECTORY_CHANGED], path);

	last_node = GTK_CTREE_ROW (row)->children;
	while (last_node){
		char *np, *text;

		gtk_ctree_node_get_pixtext (
			ctree, last_node, column, &text,
			NULL, NULL, NULL);

		np = g_concat_dir_and_file (path, text);
		gtk_dtree_load_path (dtree, np, last_node, 0);
		g_free (np);

		last_node = GTK_CTREE_NODE_NEXT (last_node);
	}

	gtk_clist_thaw (GTK_CLIST (ctree));
}

static GtkCTreeNode *
gtk_dtree_lookup_dir (GtkDTree *dtree, GtkCTreeNode *parent, char *dirname)
{
	GtkCTreeNode *node;
	
	g_assert (dtree);
	g_assert (parent);
	g_assert (dirname);

	node = GTK_CTREE_ROW (parent)->children;
	
	while (node && GTK_CTREE_ROW (node)->parent == parent){
		char *text;
		
		gtk_ctree_node_get_pixtext (
			GTK_CTREE (dtree), node, 0, &text,
			NULL, NULL, NULL);

		if (strcmp (dirname, text) == 0)
			return node;

		node = GTK_CTREE_NODE_NEXT (node);
	}

	return NULL;
}

/**
 * gtk_dtree_select_dir:
 * @dtree: the tree
 * @path:  The path we want loaded into the tree
 *
 * Attemps to open all of the tree notes until
 * path is reached.  It takes a fully qualified 
 * pathname.
 *
 * Returns: TRUE if it succeeded, otherwise, FALSE
 */
gboolean
gtk_dtree_select_dir (GtkDTree *dtree, char *path)
{
	GtkCTreeNode *current_node;
	char *s, *current, *npath;
	
	g_return_if_fail (dtree != NULL);
	g_return_if_fail (GTK_IS_DTREE (dtree));
	g_return_if_fail (path != NULL);
	g_return_if_fail (*path == '/');

	s = alloca (strlen (path)+1);
	strcpy (s, path);
	current_node = dtree->root_node;

	s++;
	npath = g_strdup ("/");
	while ((current = strtok (s, "/")) != NULL){
		char *comp, *full_path;
		GtkCTreeNode *node;
		
		full_path = g_concat_dir_and_file (npath, current);
		g_free (npath);
		npath = full_path;

		node = gtk_dtree_lookup_dir (dtree, current_node, current);
		if (!node){
			printf ("Cargando: [%s]\n", current);
			gtk_dtree_load_path (dtree, full_path, current_node, 2);

			node = gtk_dtree_lookup_dir (dtree, current_node, current);
		}
		g_free (full_path);
		
		if (node)
			current_node = node;
		else
			break;
		
		s = NULL;
	}
        g_free (npath);
}

static void
gtk_dtree_expand (GtkCTree *ctree, GtkCTreeNode *node)
{
	parent_class->tree_expand (ctree, node);
	gtk_dtree_select_row (ctree, node, 0);
}

static void
gtk_dtree_destroy (GtkObject *object)
{
	GtkDTree *dtree = GTK_DTREE (object);

	gdk_pixmap_unref (dtree->pixmap_open);
	gdk_pixmap_unref (dtree->pixmap_close);
	gdk_bitmap_unref (dtree->bitmap_open);
	gdk_bitmap_unref (dtree->bitmap_close);
	
	(GTK_OBJECT_CLASS (parent_class))->destroy (object);
}

static void
gtk_dtree_class_init (GtkDTreeClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkCTreeClass  *ctree_class  = (GtkCTreeClass *) klass;
	
	parent_class = gtk_type_class (GTK_TYPE_CTREE);

	gtk_dtree_signals [DIRECTORY_CHANGED] =
		gtk_signal_new (
			"directory_changed",
			GTK_RUN_FIRST, object_class->type,
			GTK_SIGNAL_OFFSET (GtkDTreeClass, directory_changed),
			gtk_marshal_NONE__POINTER,
			GTK_TYPE_NONE,
			1,
			GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, gtk_dtree_signals, LAST_SIGNAL);

	object_class->destroy        = gtk_dtree_destroy;
	
	ctree_class->tree_select_row = gtk_dtree_select_row;
	ctree_class->tree_expand     = gtk_dtree_expand;
}

static void
gtk_dtree_load_root_tree (GtkDTree *dtree)
{
	char *root_dir [1] = { "/" };

	g_assert (dtree);
	
	gtk_clist_freeze (GTK_CLIST (dtree));
	gtk_clist_clear (GTK_CLIST (dtree));

	dtree->root_node = gtk_ctree_insert_node (
		GTK_CTREE (dtree), NULL, NULL,
		root_dir, TREE_SPACING,
		dtree->pixmap_close,
		dtree->bitmap_close,
		dtree->pixmap_open,
		dtree->bitmap_open,
		FALSE, TRUE);

	gtk_dtree_load_path (dtree, "/", dtree->root_node, 1);

	dtree->last_node = dtree->root_node;

	if (dtree->current_path != NULL)
		g_free (dtree->current_path);

	/* Set current_path to "/" */
	dtree->current_path = g_malloc (2);
	dtree->current_path [0] = '/';
	dtree->current_path [1] = 0;

	/* Select root node */
	gtk_ctree_select (GTK_CTREE (dtree), dtree->root_node);

	gtk_clist_thaw (GTK_CLIST (dtree));
}

static void
gtk_dtree_load_pixmap (char *pix [], GdkPixmap **pixmap, GdkBitmap **bitmap)
{
	g_assert (pix);
	g_assert (pixmap);
	g_assert (bitmap);
	
	*pixmap = gdk_pixmap_colormap_create_from_xpm_d (
		NULL,		/* NULL window */
		gtk_widget_get_default_colormap (),
		bitmap,
		NULL,
		pix);
}

/*
 * FIXME: This uses the current colormap and pixmap.
 * This means that at widget *creation* time the proper
 * colormap and visual should be set, dont wait till
 * realize for that
 */
static void
gdk_dtree_load_pixmaps (GtkDTree *dtree)
{
	g_assert (dtree);
	
	gtk_dtree_load_pixmap (
		DIRECTORY_OPEN_XPM,
		&dtree->pixmap_open, &dtree->bitmap_open);
	gtk_dtree_load_pixmap (
		DIRECTORY_CLOSE_XPM,
		&dtree->pixmap_close, &dtree->bitmap_close);
}

static void
gtk_dtree_init (GtkDTree *dtree)
{
	dtree->current_path = NULL;
}

void
gtk_dtree_construct (GtkDTree *dtree)
{
	g_return_if_fail (dtree != NULL);
	g_return_if_fail (GTK_IS_DTREE (dtree));

	gtk_ctree_construct (GTK_CTREE (dtree), 1, 0, NULL);

	gtk_ctree_set_line_style (GTK_CTREE (dtree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_reorderable (GTK_CTREE (dtree), TRUE);
	gtk_clist_set_selection_mode(GTK_CLIST (dtree), GTK_SELECTION_BROWSE);
	gdk_dtree_load_pixmaps (dtree);
	gtk_dtree_load_root_tree (dtree);
	
}

GtkWidget *
gtk_dtree_new (void)
{
	GtkWidget *widget;
	
	widget = gtk_type_new (GTK_TYPE_DTREE);
	gtk_dtree_construct (GTK_DTREE (widget));

	return widget;
}

GtkType
gtk_dtree_get_type (void)
{
	static GtkType dtree_type = 0;
	
	if (!dtree_type)
	{
		GtkTypeInfo dtree_info =
		{
			"GtkDTree",
			sizeof (GtkDTree),
			sizeof (GtkDTreeClass),
			(GtkClassInitFunc) gtk_dtree_class_init,
			(GtkObjectInitFunc) gtk_dtree_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		dtree_type = gtk_type_unique (GTK_TYPE_CTREE, &dtree_info);
	}
	
	return dtree_type;
}
