/*
 * GtkDTree: A directory tree view
 *
 * Original version by Daniel Lacroix (LACROIX@wanadoo.fr)
 *
 * Adapted to the Midnight Commander by Miguel.
 *
 */
#include <config.h>
#include "global.h"
#include <gnome.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dir-open.xpm"
#include "dir-close.xpm"
#include "main.h"
#include "treestore.h"
#include "gtkdtree.h"
#include "../vfs/vfs.h"

#define FREEZE

#ifdef HACK
# define mc_opendir opendir
# define mc_closedir closedir
# define mc_stat stat
# define mc_readdir readdir
#endif

#define TREE_SPACING 3

static GtkCTreeClass *parent_class = NULL;

enum {
	DIRECTORY_CHANGED,
	SCAN_BEGIN,
	SCAN_END,
	POSSIBLY_UNGRAB,
	LAST_SIGNAL
};

static guint gtk_dtree_signals[LAST_SIGNAL] = { 0 };

char *
gtk_dtree_get_row_path (GtkDTree *dtree, GtkCTreeNode *row)
{
	char *node_text, *path;

	g_return_val_if_fail (dtree != NULL, NULL);
	g_return_val_if_fail (GTK_IS_DTREE (dtree), NULL);
	g_return_val_if_fail (row != NULL, NULL);

	path = g_strdup ("");
	do {
		char *new_path;
		int val;

		val = gtk_ctree_node_get_pixtext (
			GTK_CTREE (dtree), row, 0,
			&node_text, NULL, NULL, NULL);

		if (!val)
			return path;

		new_path = g_concat_dir_and_file (node_text, path);
		g_free (path);
		path = new_path;

		row = GTK_CTREE_ROW (row)->parent;
	} while (row);

	if (path[0] && path[1]) {
		int l = strlen (path);

		if (path[l - 1] == '/')
			path[l - 1] = 0;
	}
	return path;
}

static GtkCTreeNode *
gtk_dtree_contains (GtkDTree *dtree, GtkCTreeNode *parent, char *text)
{
	GtkCTreeNode *node;

	g_assert (dtree);
	g_assert (parent);
	g_assert (text);

	node = GTK_CTREE_ROW (parent)->children;

	for (; node && GTK_CTREE_ROW (node)->parent == parent;) {
		char *s;

		gtk_ctree_node_get_pixtext (GTK_CTREE (dtree), node, 0, &s, NULL, NULL, NULL);

		if (strcmp (s, text) == 0)
			return node;

		node = GTK_CTREE_ROW (node)->sibling;
	}

	return NULL;
}

static GtkCTreeNode *
gtk_dtree_insert_node (GtkDTree *dtree, GtkCTreeNode *parent, char *text)
{
	char *texts[1];

	texts[0] = text;

	return gtk_ctree_insert_node (GTK_CTREE (dtree), parent, NULL,
				      texts, TREE_SPACING,
				      dtree->pixmap_close,
				      dtree->bitmap_close,
				      dtree->pixmap_open,
				      dtree->bitmap_open,
				      FALSE, FALSE);
}

static gboolean
gtk_dtree_load_path (GtkDTree *dtree, char *path, GtkCTreeNode *parent, int level)
{
	GtkCTreeNode *phantom = NULL;
	tree_scan  *dir;
	tree_entry *dirent;
        struct stat dir_stat;

	g_assert (path);
	g_assert (parent);
	g_assert (dtree);

        if (mc_stat (path, &dir_stat)) {
		return FALSE;
        }
        if (!S_ISDIR(dir_stat.st_mode))
		return FALSE;

	dtree->loading_dir++;

#if 0
        phantom = gtk_dtree_contains (dtree, parent, "PHANTOM");
        if (!level) {
		dirent = tree_store_whereis (path);
		if (!phantom && (!dirent  || (dirent && !dirent->scanned)))
			if (dir_stat.st_nlink > 2 || strncmp(path,"/afs",4)==0)
				gtk_dtree_insert_node (dtree, parent, "PHANTOM");
		dtree->loading_dir--;
		return TRUE;
        }
#endif

	dir = tree_store_opendir (path);
	if (!dir) {
		dtree->loading_dir--;
		return FALSE;
	}

	while ((dirent = tree_store_readdir (dir)) != NULL) {
		GtkCTreeNode *sibling;
		char *text;

		text = x_basename (dirent->name);

		/* Do not insert duplicates */
		sibling = gtk_dtree_contains (dtree, parent, text);

		if (sibling == NULL)
			sibling = gtk_dtree_insert_node (dtree, parent, text);

		if (level)
			gtk_dtree_load_path (dtree, dirent->name, sibling, level-1);

		if (!level)
			break;
	}

	tree_store_closedir (dir);
	dtree->loading_dir--;
	if (phantom != NULL && level) {
		dtree->removing_rows = 1;
		gtk_ctree_remove_node (GTK_CTREE (dtree), phantom);
		dtree->removing_rows = 0;
	}

	return TRUE;
}

static void
scan_begin (GtkDTree *dtree)
{
	if (++dtree->scan_level == 1) {
#ifdef FREEZE
		gtk_clist_freeze (GTK_CLIST (dtree));
#endif
		gtk_signal_emit (GTK_OBJECT (dtree), gtk_dtree_signals[SCAN_BEGIN]);
	}
}

static void
scan_end (GtkDTree *dtree)
{
	g_assert (dtree->scan_level > 0);

	if (--dtree->scan_level == 0) {
		gtk_signal_emit (GTK_OBJECT (dtree), gtk_dtree_signals[SCAN_END]);
#ifdef FREEZE
		gtk_clist_thaw (GTK_CLIST (dtree));
#endif
	}
}

/* Scans a subdirectory in the tree */
static void
scan_subtree (GtkDTree *dtree, GtkCTreeNode *row, char *path)
{
	dtree->loading_dir++;
	scan_begin (dtree);

	gtk_dtree_load_path (dtree, path, row, 1);

	scan_end (dtree);
	dtree->loading_dir--;
}

static void
gtk_dtree_select_row (GtkCTree *ctree, GtkCTreeNode *row, gint column)
{
	GtkDTree *dtree;
	char *path;

	dtree = GTK_DTREE (ctree);

	if (dtree->removing_rows)
		return;

	/* Ask for someone to ungrab the mouse, as the stupid clist grabs it on
	 * button press.  We cannot do it unconditionally because we don't want
	 * to knock off a DnD grab.  We cannot do it in a button_press handler,
	 * either, because the row is selected *inside* the default handler.
	 */
	gtk_signal_emit (GTK_OBJECT (dtree), gtk_dtree_signals[POSSIBLY_UNGRAB], NULL);

	scan_begin (dtree);

	(* parent_class->tree_select_row) (ctree, row, column);

	if (row == dtree->last_node) {
		scan_end (dtree);
		return;
	}

	dtree->last_node = row;

	/* Set the new current path */

	path = gtk_dtree_get_row_path (dtree, row);

	if (dtree->current_path)
		g_free (dtree->current_path);

	dtree->current_path = path;

	scan_subtree (dtree, row, path);

	if (!dtree->internal)
		gtk_signal_emit (GTK_OBJECT (dtree), gtk_dtree_signals[DIRECTORY_CHANGED], path);

	scan_end (dtree);
}

static GtkCTreeNode *
gtk_dtree_lookup_dir (GtkDTree *dtree, GtkCTreeNode *parent, char *dirname)
{
	GtkCTreeNode *node;

	g_assert (dtree);
	g_assert (parent);
	g_assert (dirname);

	node = GTK_CTREE_ROW (parent)->children;

	while (node) {
		char *text;

		if (GTK_CTREE_ROW (node)->parent == parent) {
			gtk_ctree_node_get_pixtext (
				GTK_CTREE (dtree), node, 0, &text,
				NULL, NULL, NULL);

			if (strcmp (dirname, text) == 0)
				return node;
		}
		node = GTK_CTREE_NODE_NEXT (node);
	}

	return NULL;
}

static gboolean
gtk_dtree_do_select_dir (GtkDTree *dtree, char *path)
{
	GtkCTreeNode *current_node;
	char *s, *current, *npath;

	g_return_val_if_fail (dtree != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_DTREE (dtree), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	if (dtree->current_path && (strcmp (path, dtree->current_path) == 0))
		return TRUE;

	s = alloca (strlen (path) + 1);
	strcpy (s, path);
	current_node = dtree->root_node;

	s++;
	npath = g_strdup ("/");

	dtree->internal = 1;
	while ((current = strtok (s, "/")) != NULL) {
		char *full_path;
		GtkCTreeNode *node;

		s = NULL;
		full_path = g_concat_dir_and_file (npath, current);
		g_free (npath);
		npath = full_path;

		node = gtk_dtree_lookup_dir (dtree, current_node, current);
		if (!node) {
			gtk_dtree_load_path (dtree, full_path, current_node, 1);

			node = gtk_dtree_lookup_dir (dtree, current_node, current);
		}

		if (node) {
			gtk_ctree_expand (GTK_CTREE (dtree), node);
			current_node = node;
		} else
			break;
	}
        g_free (npath);

	if (current_node) {
		gtk_ctree_select (GTK_CTREE (dtree), current_node);
		if (gtk_ctree_node_is_visible (GTK_CTREE (dtree), current_node)
		    != GTK_VISIBILITY_FULL)
			gtk_ctree_node_moveto (GTK_CTREE (dtree), current_node, 0, 0.5, 0.0);
	}

	if (dtree->current_path) {
		g_free (dtree->current_path);
		dtree->current_path = g_strdup (path);
	}

	if (dtree->requested_path) {
		g_free (dtree->requested_path);
		dtree->requested_path = NULL;
	}

	dtree->internal = 0;

	return TRUE;
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
	g_return_val_if_fail (dtree != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_DTREE (dtree), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (*path == '/', FALSE);

	if (dtree->visible)
		gtk_dtree_do_select_dir (dtree, path);
	else {
		if (dtree->requested_path)
			g_free (dtree->requested_path);
		dtree->requested_path = g_strdup (path);
	}

	return TRUE;
}

static void
gtk_dtree_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkDTree *dtree = GTK_DTREE (widget);
	char *request;

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	if (allocation->width > 1 && allocation->height > 1)
		dtree->visible = TRUE;
	else
		dtree->visible = FALSE;

	if (!(dtree->visible && dtree->requested_path))
		return;

	if (!dtree->visible)
		return;

	if (!dtree->requested_path)
		return;

	if (strcmp (dtree->current_path, dtree->requested_path) == 0) {
		g_free (dtree->requested_path);
		dtree->requested_path = NULL;
		return;
	}

	request = dtree->requested_path;
	dtree->requested_path = NULL;
	gtk_dtree_do_select_dir (dtree, request);
	g_free (request);
}

/* Our handler for the tree_expand signal */
static void
gtk_dtree_expand (GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkDTree *dtree;
	char *path;

	dtree = GTK_DTREE (ctree);

	scan_begin (dtree);

	(* parent_class->tree_expand) (ctree, node);

	path = gtk_dtree_get_row_path (dtree, node);
	scan_subtree (dtree, node, path);
	g_free (path);

	scan_end (dtree);
}

/* Our handler for the tree_collapse signal */
static void
gtk_dtree_collapse (GtkCTree *ctree, GtkCTreeNode *node)
{
	GList *sel;
	int do_select;

	/* Select the node only if it is an ancestor of the currently-selected
	 * node.
	 */

	do_select = FALSE;

	sel = GTK_CLIST (ctree)->selection;
	if (!sel)
		do_select = TRUE;
	else {
		if (node != sel->data && gtk_dtree_is_ancestor (GTK_DTREE (ctree), node, sel->data))
			do_select = TRUE;
	}

	(* parent_class->tree_collapse) (ctree, node);

	if (do_select)
		gtk_ctree_select (ctree, node);
}

/*
 * entry_removed_callback:
 *
 * Called when an entry is removed by the treestore
 */
static void
entry_removed_callback (tree_entry *tree, void *data)
{
	GtkCTreeNode *current_node;
	GtkDTree *dtree = data;
	char *dirname, *copy, *current;

	if (dtree->loading_dir)
		return;

	copy = dirname = g_strdup (tree->name);
	copy++;
	current_node = dtree->root_node;
	while ((current = strtok (copy, "/")) != NULL) {
		current_node = gtk_dtree_lookup_dir (dtree, current_node, current);
		if (!current_node)
			break;
		copy = NULL;
	}
	if (current == NULL && current_node) {
		dtree->removing_rows = 1;
		gtk_ctree_remove_node (GTK_CTREE (data), current_node);
		dtree->removing_rows = 0;
	}

	g_free (dirname);
}

/*
 * entry_added_callback:
 *
 * Callback invoked by the treestore when a tree_entry has been inserted
 * into the treestore.  We update the gtkdtree with this new information.
 */
static void
entry_added_callback (char *dirname, void *data)
{
	GtkCTreeNode *current_node, *new_node;
	GtkDTree *dtree = data;
	char *copy, *current, *npath, *full_path;

	if (dtree->loading_dir)
		return;

	dirname = g_strdup (dirname);
	copy = dirname;
	copy++;
	current_node = dtree->root_node;
	npath = g_strdup ("/");
	while ((current = strtok (copy, "/")) != NULL) {
		full_path = g_concat_dir_and_file (npath, current);
		g_free (npath);
		npath = full_path;

		new_node = gtk_dtree_lookup_dir (dtree, current_node, current);
		if (!new_node) {
			GtkCTreeNode *sibling;

			sibling = gtk_dtree_insert_node (dtree, current_node, current);
			gtk_dtree_load_path (dtree, full_path, sibling, 1);
			break;
		}
		copy = NULL;
		current_node = new_node;
	}
	g_free (npath);
	g_free (dirname);
}

/*
 * Callback routine invoked from the treestore to hint us
 * about the progress of the freezing
 */
static void
tree_set_freeze (int state, void *data)
{
	GtkDTree *dtree = GTK_DTREE (data);

#ifdef FREEZE
	if (state)
		gtk_clist_freeze (GTK_CLIST (dtree));
	else
		gtk_clist_thaw (GTK_CLIST (dtree));
#endif
}

static void
gtk_dtree_destroy (GtkObject *object)
{
	GtkDTree *dtree = GTK_DTREE (object);

	tree_store_remove_entry_remove_hook (entry_removed_callback);
	tree_store_remove_entry_add_hook (entry_added_callback);
	tree_store_remove_freeze_hook (tree_set_freeze);

	gdk_pixmap_unref (dtree->pixmap_open);
	gdk_pixmap_unref (dtree->pixmap_close);
	gdk_bitmap_unref (dtree->bitmap_open);
	gdk_bitmap_unref (dtree->bitmap_close);

	if (dtree->current_path)
		g_free (dtree->current_path);

	if (dtree->requested_path)
		g_free (dtree->requested_path);

	(GTK_OBJECT_CLASS (parent_class))->destroy (object);
}

static void
gtk_dtree_class_init (GtkDTreeClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkCTreeClass  *ctree_class  = (GtkCTreeClass *) klass;

	parent_class = gtk_type_class (GTK_TYPE_CTREE);

	gtk_dtree_signals[DIRECTORY_CHANGED] =
		gtk_signal_new ("directory_changed",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GtkDTreeClass, directory_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE,
				1,
				GTK_TYPE_POINTER);
	gtk_dtree_signals[SCAN_BEGIN] =
		gtk_signal_new ("scan_begin",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GtkDTreeClass, scan_begin),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	gtk_dtree_signals[SCAN_END] =
		gtk_signal_new ("scan_end",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GtkDTreeClass, scan_end),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	gtk_dtree_signals[POSSIBLY_UNGRAB] =
		gtk_signal_new ("possibly_ungrab",
				GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GtkDTreeClass, possibly_ungrab),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);

	gtk_object_class_add_signals (object_class, gtk_dtree_signals, LAST_SIGNAL);

	object_class->destroy = gtk_dtree_destroy;

	widget_class->size_allocate = gtk_dtree_size_allocate;

	ctree_class->tree_select_row = gtk_dtree_select_row;
	ctree_class->tree_expand = gtk_dtree_expand;
	ctree_class->tree_collapse = gtk_dtree_collapse;
}

static void
gtk_dtree_load_root_tree (GtkDTree *dtree)
{
	char *root_dir[1] = { "/" };

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
	dtree->current_path[0] = '/';
	dtree->current_path[1] = 0;

	/* Select root node */
	gtk_ctree_select (GTK_CTREE (dtree), dtree->root_node);
	gtk_clist_thaw (GTK_CLIST (dtree));
}

static void
gtk_dtree_load_pixmap (char *pix[], GdkPixmap **pixmap, GdkBitmap **bitmap)
{
	GdkImlibImage *image;

	g_assert (pix);
	g_assert (pixmap);
	g_assert (bitmap);

	image = gdk_imlib_create_image_from_xpm_data (pix);
	*pixmap = NULL;
	*bitmap = NULL;
	g_return_if_fail(image);

	gdk_imlib_render (image, image->rgb_width, image->rgb_height);
	*pixmap = gdk_imlib_move_image (image);
	*bitmap = gdk_imlib_move_mask (image);
}

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

static int dirty_tag = -1;

static int
gtk_dtree_save_tree (void)
{
	dirty_tag = -1;
	tree_store_save ();
	return FALSE;
}

/*
 * Callback routine invoked by the treestore code when the state
 * of the treestore has been modified.
 */
static void
gtk_dtree_dirty_notify (int state)
{
	if (dirty_tag != -1) {
		if (state)
			return;
		else {
			gtk_timeout_remove (dirty_tag);
			dirty_tag = -1;
		}
	}

	if (state)
		dirty_tag = gtk_timeout_add (10 * 1000, (GtkFunction) gtk_dtree_save_tree, NULL);
}

static void
gtk_dtree_init (GtkDTree *dtree)
{
	/* HACK: This is to avoid GtkCTree's broken focusing behavior */
	GTK_WIDGET_UNSET_FLAGS (dtree, GTK_CAN_FOCUS);

	dtree->current_path = NULL;
	dtree->auto_expanded_nodes = NULL;

	tree_store_dirty_notify = gtk_dtree_dirty_notify;

	tree_store_add_entry_remove_hook (entry_removed_callback, dtree);
	tree_store_add_entry_add_hook (entry_added_callback, dtree);
	tree_store_add_freeze_hook (tree_set_freeze, dtree);
}

void
gtk_dtree_construct (GtkDTree *dtree)
{
	GtkCList *clist;
	GtkCTree *ctree;

	g_return_if_fail (dtree != NULL);
	g_return_if_fail (GTK_IS_DTREE (dtree));

	clist = GTK_CLIST (dtree);
	ctree = GTK_CTREE (dtree);

	gtk_ctree_construct (ctree, 1, 0, NULL);

	gtk_clist_set_selection_mode(clist, GTK_SELECTION_BROWSE);
	gtk_clist_set_auto_sort (clist, TRUE);
	gtk_clist_set_sort_type (clist, GTK_SORT_ASCENDING);
	gtk_clist_set_column_auto_resize (clist, 0, TRUE);
	gtk_clist_columns_autosize (clist);

	gtk_ctree_set_line_style (ctree, GTK_CTREE_LINES_DOTTED);
	gtk_clist_set_reorderable (GTK_CLIST (ctree), FALSE);

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

/**
 * gtk_dtree_is_ancestor:
 * @dtree: A tree
 * @node: The presumed ancestor node
 * @child: The presumed child node
 *
 * Tests whether a node is an ancestor of a child node.  This does this in
 * O(height of child), instead of O(number of children in node), like GtkCTree
 * does.
 *
 * Return value: TRUE if the node is an ancestor of the child, FALSE otherwise.
 **/
gboolean
gtk_dtree_is_ancestor (GtkDTree *dtree, GtkCTreeNode *node, GtkCTreeNode *child)
{
	g_return_val_if_fail (dtree != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_DTREE (dtree), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	g_return_val_if_fail (child != NULL, FALSE);

	for (; child; child = GTK_CTREE_ROW (child)->parent)
		if (child == node)
			return TRUE;

	return FALSE;
}
