/* Icon loading support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"
#include "dialog.h"

#include <gnome.h>
#include "gicon.h"


/* What kinds of images can an icon set contain */
typedef enum {
	ICON_TYPE_PLAIN,
	ICON_TYPE_SYMLINK,
	ICON_TYPE_STALLED
} IconType;

/* Information for an icon set (plain icon plus overlayed versions) */
typedef struct {
	GdkImlibImage *plain;		/* Plain version */
	GdkImlibImage *symlink;		/* Symlink version */
	GdkImlibImage *stalled;		/* Stalled symlink version */
	char *filename;			/* Filename for the plain image */
} IconSet;

static int gicon_inited;		/* Has the icon system been inited? */

static GHashTable *name_hash;		/* Hash from filename -> IconSet */
static GHashTable *image_hash;		/* Hash from imlib image -> IconSet */

static GdkImlibImage *symlink_overlay;	/* The little symlink overlay */
static GdkImlibImage *stalled_overlay;	/* The little stalled symlink overlay */

/* Default icons, guaranteed to exist */
static IconSet *iset_directory;
static IconSet *iset_dirclosed;
static IconSet *iset_executable;
static IconSet *iset_regular;
static IconSet *iset_core;
static IconSet *iset_sock;
static IconSet *iset_fifo;
static IconSet *iset_chardev;
static IconSet *iset_blockdev;

/* Our UID and GID, needed to see if the user can access some files */
static uid_t our_uid;
static gid_t our_gid;


/* Whether we should always use (expensive) metadata lookups for file panels or not */
int we_can_afford_the_speed = 0;


/* Builds a composite of the plain image and the litle symlink icon */
static GdkImlibImage *
build_overlay (GdkImlibImage *plain, GdkImlibImage *overlay)
{
	int rowstride;
	int overlay_rowstride;
	guchar *src, *dest;
	int y;
	GdkImlibImage *im;

	im = gdk_imlib_clone_image (plain);

	rowstride = plain->rgb_width * 3;
	overlay_rowstride = overlay->rgb_width * 3;

	dest = im->rgb_data + ((plain->rgb_height - overlay->rgb_height) * rowstride
			       + (plain->rgb_width - overlay->rgb_width) * 3);

	src = overlay->rgb_data;

	for (y = 0; y < overlay->rgb_height; y++) {
		memcpy (dest, src, overlay_rowstride);

		dest += rowstride;
		src += overlay_rowstride;
	}

	gdk_imlib_changed_image (im);
	return im;
}

/* Ensures that the icon set has the requested image */
static void
ensure_icon_image (IconSet *iset, IconType type)
{
	g_assert (iset->plain != NULL);

	switch (type) {
	case ICON_TYPE_PLAIN:
		/* The plain type always exists, so do nothing */
		break;

	case ICON_TYPE_SYMLINK:
		iset->symlink = build_overlay (iset->plain, symlink_overlay);
		g_hash_table_insert (image_hash, iset->symlink, iset);
		break;

	case ICON_TYPE_STALLED:
		iset->stalled = build_overlay (iset->plain, stalled_overlay);
		g_hash_table_insert (image_hash, iset->stalled, iset);
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
compute_scaled_size (int width, int height, int *nwidth, int *nheight)
{
	g_return_if_fail (nwidth != NULL);
	g_return_if_fail (nheight != NULL);
	
	if (width <= ICON_IMAGE_WIDTH && height <= ICON_IMAGE_HEIGHT) {
		*nheight = height;
		*nwidth = width;
	} else if (width < height) {
		*nheight = ICON_IMAGE_HEIGHT;
		*nwidth = *nheight * width / height;
	} else {
		*nwidth = ICON_IMAGE_WIDTH;
		*nheight = *nwidth * height / width;
	}
}
		
/* Returns a newly allocated, correctly scaled image */
static GdkImlibImage *
get_scaled_image (GdkImlibImage *orig)
{
	GdkImlibImage *im;
	int width, height;
	
	g_return_val_if_fail (orig != NULL, NULL);
	
	compute_scaled_size (orig->rgb_width, orig->rgb_height,
			     &width, &height);
	im = gdk_imlib_clone_scaled_image (orig, width, height);

	return im;
}

/* Returns the icon set corresponding to the specified image.
 * If we create a new IconSet, iset->plain is set to a new scaled
 * image, so _WE FREE THE IM PARAMETER_. */
static IconSet *
get_icon_set_from_image (GdkImlibImage *im)
{
	IconSet *iset;

	g_return_val_if_fail (im != NULL, NULL);
	
	iset = g_hash_table_lookup (image_hash, im);
	if (iset)
		return iset;

	iset = g_new (IconSet, 1);
	iset->plain = get_scaled_image (im);
	iset->symlink = NULL;
	iset->stalled = NULL;
	iset->filename = NULL;
	
	
	/* Insert the icon information into the hash tables */
	
	g_hash_table_remove (image_hash, im);
	g_hash_table_insert (image_hash, iset->plain, iset);

	gdk_imlib_destroy_image (im);

	return iset;
}

/* Returns the icon set corresponding to the specified icon filename, or NULL if
 * the file could not be loaded.
 */
static IconSet *
get_icon_set (const char *filename)
{
	GdkImlibImage *im;
	IconSet *iset;
	
	iset = g_hash_table_lookup (name_hash, filename);
	if (iset)
		return iset;

	im = gdk_imlib_load_image ((char *) filename);
	if (!im)
		return NULL;
	
	iset = get_icon_set_from_image (im);
	im = NULL;

	iset->filename = g_strdup (filename);

	/* Insert the icon information into the hash tables */

	g_hash_table_insert (name_hash, iset->filename, iset);
	
	return iset;
}

/* Die because the icon installation is wrong */
static void
die_with_no_icons (void)
{
	message (1, _("Error"), _("Default set of icons not found, please check your installation"));
	exit (1);
}

/* Convenience function to load one of the default icons and die if this fails */
static IconSet *
get_stock_icon (char *name)
{
	char *filename;
	IconSet *iset;

	filename = g_concat_dir_and_file (ICONDIR, name);
	iset = get_icon_set (filename);
	g_free (filename);

	if (!iset)
		die_with_no_icons ();

	return iset;
}

/* Convenience function to load one of the default overlays and die if this fails */
static GdkImlibImage *
get_stock_overlay (char *name)
{
	char *filename;
	GdkImlibImage *im;

	filename = g_concat_dir_and_file (ICONDIR, name);
	im = gdk_imlib_load_image (filename);
	g_free (filename);

	if (!im)
		die_with_no_icons ();

	return im;
		
}

/**
 * gicon_init:
 * @void: 
 * 
 * Initializes the icon module.
 **/
void
gicon_init (void)
{
	if (gicon_inited)
		return;

	gicon_inited = TRUE;

	name_hash = g_hash_table_new (g_str_hash, g_str_equal);
	image_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Load the default icons */

	iset_directory  = get_stock_icon ("i-directory.png");
	iset_dirclosed  = get_stock_icon ("i-dirclosed.png");
	iset_executable = get_stock_icon ("i-executable.png");
	iset_regular    = get_stock_icon ("i-regular.png");
	iset_core       = get_stock_icon ("i-core.png");
	iset_sock       = get_stock_icon ("i-sock.png");
	iset_fifo       = get_stock_icon ("i-fifo.png");
	iset_chardev    = get_stock_icon ("i-chardev.png");
	iset_blockdev   = get_stock_icon ("i-blockdev.png");

	/* Load the overlay icons */

	symlink_overlay = get_stock_overlay ("i-symlink.png");
	stalled_overlay = get_stock_overlay ("i-stalled.png");

	our_uid = getuid ();
	our_gid = getgid ();
}

/* Tries to get an icon from the file's metadata information */
static GdkImlibImage *
get_icon_from_metadata (char *filename)
{
	int size;
	char *buf;
	IconSet *iset = NULL;
	
	/* Try the inlined icon */

	if (gnome_metadata_get (filename, "icon-inline-png", &size, &buf) == 0) {
		GdkImlibImage *im;
		im = gdk_imlib_inlined_png_to_image (buf, size);
		g_free (buf);
		if (im) {
			iset = get_icon_set_from_image (im);
			im = NULL;
		}
	}

	/* Try the non-inlined icon */

	if (!iset && gnome_metadata_get (filename, "icon-filename", &size, &buf) == 0) {
		iset = get_icon_set (buf);
		g_free (buf);
	}
	
	if (iset) {
		ensure_icon_image (iset, ICON_TYPE_PLAIN);
		return iset->plain;
	}

	return NULL; /* nothing is available */
}

/* Returns whether we are in the specified group or not */
static int
we_are_in_group (gid_t gid)
{
	gid_t *groups;
	int ngroups, i;
	int retval;

	if (our_gid == gid)
		return TRUE;

	ngroups = getgroups (0, NULL);
	if (ngroups == -1 || ngroups == 0)
		return FALSE;

	groups = g_new (gid_t, ngroups);
	ngroups = getgroups (ngroups, groups);
	if (ngroups == -1) {
		g_free (groups);
		return FALSE;
	}

	retval = FALSE;

	for (i = 0; i < ngroups; i++)
		if (groups[i] == gid) 
			retval = TRUE;

	g_free (groups);
	return retval;
}

/* Returns whether we can access the contents of directory specified by the file entry */
static int
can_access_directory (file_entry *fe)
{
	mode_t test_mode;

	if (fe->buf.st_uid == our_uid)
		test_mode = S_IRUSR | S_IXUSR;
	else if (we_are_in_group (fe->buf.st_gid))
		test_mode = S_IRGRP | S_IXGRP;
	else
		test_mode = S_IROTH | S_IXOTH;

	return (fe->buf.st_mode & test_mode) == test_mode;
}

/* This is the last resort for getting a file's icon.  It uses the file mode
 * bits or a hardcoded name.
 */
static IconSet *
get_default_icon (file_entry *fe)
{
	mode_t mode = fe->buf.st_mode;

	if (S_ISSOCK (mode))
		return iset_sock;

	if (S_ISCHR (mode))
		return iset_chardev;

	if (S_ISBLK (mode))
		return iset_blockdev;

	if (S_ISFIFO (mode))
		return iset_fifo;

	if (is_exe (mode))
		return iset_executable;

	if (!strcmp (fe->fname, "core") || !strcmp (extension (fe->fname), "core"))
		return iset_core;

	return iset_regular; /* boooo */
}

/**
 * gicon_get_icon_for_file:
 * @directory: The directory on which the file resides.
 * @fe: The file entry that represents the file.
 * @do_quick: Whether the function should try to use (expensive) metadata information.
 * 
 * Returns the appropriate icon for the specified file.
 * 
 * Return value: The icon for the specified file.
 **/
GdkImlibImage *
gicon_get_icon_for_file (char *directory, file_entry *fe, gboolean do_quick)
{
	IconSet *iset;
	mode_t mode;
	const char *mime_type;
	gboolean is_user_set = FALSE;

	g_return_val_if_fail (directory != NULL, NULL);
	g_return_val_if_fail (fe != NULL, NULL);

	gicon_init ();

	mode = fe->buf.st_mode;

	/* 1. First try the user's settings */

	if (!do_quick || we_can_afford_the_speed) {
		char *full_name;
		GdkImlibImage *im;

		full_name = g_concat_dir_and_file (directory, fe->fname);
		im = get_icon_from_metadata (full_name);
		g_free (full_name);
		
		if (im) {
			iset = get_icon_set_from_image (im);
			im = NULL;
			is_user_set = TRUE;
			goto add_link;
		}
	}
	
	/* 2. Before we do anything else, make sure the 
	 * pointed-to file exists if a link */
	
	if (S_ISLNK (mode) && fe->f.stalled_link) {
		const char *icon_name;

		icon_name = gnome_unconditional_pixmap_file ("gnome-warning.png");
		if (icon_name) {
			iset = get_icon_set (icon_name);
			if (iset)
				goto add_link;
		}
	}

	/* 3. See if it is a directory */
	
	if (S_ISDIR (mode)) {
		if (can_access_directory (fe))
			iset = iset_directory;
		else
			iset = iset_dirclosed;

		goto add_link;
	}

	/* 4. Try MIME-types */

	mime_type = gnome_mime_type_or_default (fe->fname, NULL);
	if (mime_type) {
		const char *icon_name;

		icon_name = gnome_mime_get_value (mime_type, "icon-filename");
		if (icon_name) {
			iset = get_icon_set (icon_name);
			if (iset)
				goto add_link;
		}
	}

	/* 5. Get an icon from the file mode */

	iset = get_default_icon (fe);

 add_link:

	g_assert (iset != NULL);

	if (S_ISLNK (mode)) {
		if (fe->f.link_to_dir && !is_user_set)
			iset = iset_directory;

		if (fe->f.stalled_link) {
			ensure_icon_image (iset, ICON_TYPE_STALLED);
			return iset->stalled;
		} else {
			ensure_icon_image (iset, ICON_TYPE_SYMLINK);
			return iset->symlink;
		}
	} else {
		ensure_icon_image (iset, ICON_TYPE_PLAIN);
		return iset->plain;
	}
}

/**
 * gicon_get_filename_for_icon:
 * @image: An icon image loaded by the icon module.
 * 
 * Queries the icon database for the icon filename that corresponds to the
 * specified image.
 * 
 * Return value: The filename that contains the icon for the specified image.
 **/
const char *
gicon_get_filename_for_icon (GdkImlibImage *image)
{
	IconSet *iset;

	g_return_val_if_fail (image != NULL, NULL);

	gicon_init ();

	iset = g_hash_table_lookup (image_hash, image);
	g_assert (iset != NULL);
	return iset->filename;
}
