/*
 * Icon loading support for the Midnight Commander
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */

#include <config.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "dir.h"
#include "util.h"
#include "dialog.h"


#include <gnome.h>
#include "gicon.h"

static GHashTable *icon_hash;
static int gicon_inited = FALSE;

/* These are some default images used in the Icon View */
static GdkImlibImage *icon_view_directory;
static GdkImlibImage *icon_view_dirclosed;
static GdkImlibImage *icon_view_executable;
static GdkImlibImage *icon_view_symlink;
static GdkImlibImage *icon_view_regular;
static GdkImlibImage *icon_view_core;
static GdkImlibImage *icon_view_sock;
static GdkImlibImage *icon_view_char_dev;
static GdkImlibImage *icon_view_block_dev;
static GdkImlibImage *icon_view_stalled;

/* Our UID and GID */
static uid_t our_uid;
static gid_t our_gid;

/*
 * If true, we choose the icon in a way that might be a bit slow
 */
static int we_can_affort_the_speed = 0;

/**
 * gicon_init:
 *
 * Initializes the hash tables for the icons used in the IconList
 * view
 */
static void
gicon_init (void)
{
	g_assert (gicon_inited == FALSE);
	
	icon_hash = g_hash_table_new (g_str_hash, g_str_equal);
	gicon_inited = TRUE;

	our_uid = getuid ();
	our_gid = getgid ();
	
	/* Recursive call to load the stock images */
	icon_view_directory  = gicon_stock_load	("i-directory.png");
	icon_view_dirclosed  = gicon_stock_load	("i-dirclosed.png");
	icon_view_executable = gicon_stock_load	("i-executable.png");
	icon_view_symlink    = gicon_stock_load	("i-symlink.png");
	icon_view_regular    = gicon_stock_load	("i-regular.png");
	icon_view_core       = gicon_stock_load	("i-core.png");
	icon_view_sock       = gicon_stock_load	("i-sock.png");
	icon_view_char_dev   = gicon_stock_load	("i-chardev.png");
	icon_view_block_dev  = gicon_stock_load	("i-blockdev.png");
	icon_view_stalled    = gicon_stock_load ("i-stalled.png");
	
	if (icon_view_directory  == NULL ||
	    icon_view_dirclosed  == NULL ||
	    icon_view_executable == NULL ||
	    icon_view_symlink    == NULL ||
	    icon_view_regular    == NULL ||
	    icon_view_core       == NULL ||
	    icon_view_sock       == NULL ||
	    icon_view_char_dev   == NULL ||
	    icon_view_block_dev  == NULL ||
	    icon_view_stalled    == NULL){
		message (1, _("Error"), _("Default set of icons not found, check your installation"));
		exit (1);
	}
}

/**
 * gicon_get_by_filename:
 *
 * Fetches an icon given an absolute pathname
 */
GdkImlibImage *
gicon_get_by_filename (char *fname)
{
	GdkImlibImage *icon;

	g_return_val_if_fail (fname != NULL, NULL);
	
	if (!gicon_inited)
		gicon_init ();

	icon = g_hash_table_lookup (icon_hash, fname);

	if (icon)
		return icon;

	icon = gdk_imlib_load_image (fname);

	if (icon)
		g_hash_table_insert (icon_hash, g_strdup (fname), icon);

	return icon;
}

/**
 * gicon_stock_load:
 *
 * Loads an icon from the Midnight Commander installation directory
 */
GdkImlibImage *
gicon_stock_load (char *basename)
{
	GdkImlibImage *im;
	char *fullname;

	g_return_val_if_fail (basename != NULL, NULL);

	fullname = concat_dir_and_file (ICONDIR, basename);
	im = gicon_get_by_filename (fullname);
	g_free (fullname);
	
	return im;
}

/**
 * gnome_file_entry_color:
 *
 * Returns an Imlib image appropiate for use in the icon list
 * based on the file_entry stat field and the filename.  This
 * routine always succeeds.x
 */
static GdkImlibImage *
gnome_file_entry_color (file_entry *fe)
{
	mode_t mode = fe->buf.st_mode;

	if (S_ISSOCK (mode))
		return icon_view_sock;

	if (S_ISCHR (mode))
		return icon_view_char_dev;

	if (S_ISBLK (mode))
		return icon_view_block_dev;

	if (S_ISFIFO (mode))
		return icon_view_sock;

	if (is_exe (mode))
		return icon_view_executable;

	if (fe->fname && (!strcmp (fe->fname, "core") || !strcmp (extension (fe->fname), "core")))
		return icon_view_core;

	return icon_view_regular;
}

/**
 * gicon_get_icon_for_file:
 *
 * Given a filename and its stat information, we return the optimal
 * icon for it.  Including a lookup in the metadata.
 */
GdkImlibImage *
gicon_get_icon_for_file_speed (file_entry *fe, gboolean do_quick)
{
	GdkImlibImage *image;
	int            size;
	char          *buf, *mime_type;
	mode_t        mode;
	
	g_return_val_if_fail (fe != NULL, NULL);

	if (!gicon_inited)
		gicon_init ();

	mode = fe->buf.st_mode;
	
	/*
	 * 1. First test for it being a directory or a link to a directory.
	 */
	if (S_ISDIR (mode)){
		if (fe->buf.st_uid != our_uid){
			if (fe->buf.st_gid != our_gid){

				/*
				 * We do not share the UID or the GID,
				 * test for read/execute permissions
				 */
				if ((mode & (S_IROTH | S_IXOTH)) != (S_IROTH | S_IXOTH))
					return icon_view_dirclosed;
			} else {

				/*
				 * Same group, check if we have permissions
				 */
				if ((mode & (S_IRGRP | S_IXGRP)) != (S_IRGRP | S_IXGRP))
					return icon_view_dirclosed;
			}
		} else {
			if ((mode & (S_IRUSR | S_IXUSR)) != (S_IRUSR | S_IXUSR))
				return icon_view_dirclosed;
		}

		return icon_view_directory;
	}
	
	if (S_ISLNK (mode)){
		if (fe->f.link_to_dir)
			return icon_view_directory;

		if (fe->f.stalled_link)
			return icon_view_stalled;

		return icon_view_symlink;
	}

	/*
	 * 2. Expensive tests
	 */
	if (!do_quick || we_can_affort_the_speed){
		/*
		 * 2.1 Try to fetch the icon as an inline png from the metadata.
		 */
		if (gnome_metadata_get (fe->fname, "icon-inline-png", &size, &buf) == 0){
			image = gdk_imlib_inlined_png_to_image (buf, size);
			
			free (buf);
			
			if (image)
				return image;
		}
		
		/*
		 * 2.2. Try to fetch the icon from the metadata.
		 */
		if (gnome_metadata_get (fe->fname, "icon-filename", &size, &buf) == 0){
			image = gicon_get_by_filename (buf);

			free (buf);
			
			if (image)
				return image;
		}
	}
	
	/*
	 * 3. Mime-type based
	 */
	mime_type = gnome_mime_type_or_default (fe->fname, NULL);
	if (mime_type){
		char *icon;

		icon = gnome_mime_get_value (mime_type, "icon-filename");

		if (icon){
			image = gicon_get_by_filename (icon);

			if (image)
				return image;
		}
	}
	
	/*
	 * 4. Try to find an appropiate icon from the stat information or
	 * the hard coded filename.
	 */
	image = gnome_file_entry_color (fe);
	
	g_assert (image != NULL);

	return image;
}

GdkImlibImage *
gicon_get_icon_for_file (file_entry *fe)
{
	gicon_get_icon_for_file_speed (fe, TRUE);
}
