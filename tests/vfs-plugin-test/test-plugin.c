/*
 * Minimal VFS test plugin.
 *
 * Registers a dummy "testvfs" class that does nothing useful,
 * but proves the dynamic loading mechanism works.
 */

#include <stdio.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"

static struct vfs_class test_vfs_class;

void
mc_vfs_plugin_init (void)
{
    fprintf (stderr, "TEST VFS PLUGIN: mc_vfs_plugin_init() called\n");

    vfs_init_class (&test_vfs_class, "testvfs", VFSF_UNKNOWN, "test");

    if (vfs_register_class (&test_vfs_class))
        fprintf (stderr, "TEST VFS PLUGIN: registered successfully with prefix 'test:'\n");
    else
        fprintf (stderr, "TEST VFS PLUGIN: registration FAILED\n");
}
