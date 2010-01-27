
/** \file  fileloc.h
 *  \brief Header: config files list
 *
 *  This file defines the locations of the various user specific
 *  configuration files of the Midnight Commander. Historically the
 *  system wide and the user specific file names have not always been
 *  the same, so don't use these names for finding system wide
 *  configuration files.
 *
 *  \todo This inconsistency should disappear in the one of the next versions (5.0?)
 */

#ifndef MC_FILELOC_H
#define MC_FILELOC_H

#ifndef MC_USERCONF_DIR
#define MC_USERCONF_DIR		".mc"
#endif


#define TAGS_NAME		"TAGS"

#define MC_GLOBAL_CONFIG_FILE	"mc.lib"
#define MC_GLOBAL_MENU		"mc.menu"
#define MC_LOCAL_MENU		".mc.menu"
#define MC_HINT			"mc.hint"
#define GLOBAL_KEYMAP_FILE	"mc.keymap"
#define CHARSETS_INDEX		"mc.charsets"
#define MC_LIB_EXT		"mc.ext"

#define MC_EXTFS_DIR		"extfs.d"

#define MC_BASHRC_FILE		"bashrc"
#define MC_CONFIG_FILE		"ini"
#define MC_FILEBIND_FILE	"bindings"
#define MC_FILEPOS_FILE		"filepos"
#define MC_HISTORY_FILE		"history"
#define MC_HOTLIST_FILE		"hotlist"
#define MC_USERMENU_FILE	"menu"
#define MC_TREESTORE_FILE	"Tree"
#define MC_PANELS_FILE		"panels.ini"
#define MC_FHL_INI_FILE		"filehighlight.ini"
#define MC_SKINS_SUBDIR		"skins"

/* editor home directory */
#define EDIT_DIR		MC_USERCONF_DIR PATH_SEP_STR "cedit"

/* file names */
#define EDIT_SYNTAX_FILE	EDIT_DIR PATH_SEP_STR "Syntax"
#define EDIT_CLIP_FILE		EDIT_DIR PATH_SEP_STR "cooledit.clip"
#define EDIT_MACRO_FILE		EDIT_DIR PATH_SEP_STR "cooledit.macros"
#define EDIT_BLOCK_FILE		EDIT_DIR PATH_SEP_STR "cooledit.block"
#define EDIT_TEMP_FILE		EDIT_DIR PATH_SEP_STR "cooledit.temp"

#define EDIT_GLOBAL_MENU	"cedit.menu"
#define EDIT_LOCAL_MENU		".cedit.menu"
#define EDIT_HOME_MENU		EDIT_DIR PATH_SEP_STR "menu"

#endif
