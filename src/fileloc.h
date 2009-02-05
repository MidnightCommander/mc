
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

#define MC_USERCONF_DIR		".mc"

#define MC_BASHRC_FILE		"bashrc"
#define MC_CONFIG_FILE		"ini"
#define MC_FILEBIND_FILE	"bindings"
#define MC_FILEPOS_FILE		"filepos"
#define MC_HISTORY_FILE		"history"
#define MC_HOTLIST_FILE		"hotlist"
#define MC_USERMENU_FILE	"menu"

#endif
