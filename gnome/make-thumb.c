#include <config.h>
#include <gnome.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

char *tmp_dir, *secure;

static void
get_temp_dir (void)
{
	int i = 5;		/* 5 attempts */
	static char name [100];

	do {
		strcpy (name, "/tmp/.thumbXXXXXX");
		tmp_dir = mktemp (name);
		if (mkdir (tmp_dir, 0700) == 0){
			secure = g_concat_dir_and_file (tmp_dir, "thumb.png");
			return;
		}
	} while (i--);
}

static void
put_in_metadata (char *dest, char *png_file)
{
	int f;
	struct stat buf;
	char *buffer;
	
	f = open (secure, O_RDONLY);
	if (!f){
		printf ("can not open the source file\n");
		exit (1);
	}

	if (stat (png_file, &buf) == -1){
		printf ("can not stat the source file\n");
		exit (1);
	}

	buffer = g_malloc (buf.st_size);
	if (buffer == NULL){
		printf ("No memory\n");
		exit (1);
	}

	if (read (f, buffer, buf.st_size) != buf.st_size){
		printf ("Something is wrong\n");
		exit (1);
	}
	
	gnome_metadata_set (dest, "icon-inline-png", buf.st_size, buffer);
	
	g_free (buffer);
}

static void
make_thumbnail (char *filename)
{
	GdkImlibImage *im, *im2;
	GdkImlibSaveInfo si;

	im = gdk_imlib_load_image (filename);
	if (!im)
		return;


	si.quality = 1;
	si.scaling = 1;
	si.xjustification = 0;
	si.yjustification = 0;
	si.page_size = 0;
	si.color = 0;

	if (im->rgb_width > im->rgb_height)
		im2 = gdk_imlib_clone_scaled_image (im, 48, im->rgb_height * 48 / im->rgb_width);
	else
		im2 = gdk_imlib_clone_scaled_image (im, im->rgb_height * 48 / im->rgb_width, 48);

	gdk_imlib_save_image (im2, secure, &si);

	put_in_metadata (filename, secure);

	gdk_imlib_destroy_image (im);
	gdk_imlib_destroy_image (im2);
}

int
main (int argc, char *argv [])
{
	int i;

	gnome_init ("test", "test", argc, argv);
	get_temp_dir ();

	for (i = 1; i < argc; i++)
		make_thumbnail (argv [i]);

	unlink (secure);
	rmdir (tmp_dir);
	return 0;
}
