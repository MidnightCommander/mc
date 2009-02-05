/*
   Provides a log file to ease tracing the program.

   Copyright (C) 2006 Roland Illig <roland.illig@gmx.de>.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#include <mhl/memory.h>
#include <mhl/types.h>

#include "global.h"
#include "logging.h"
#include "setup.h"

/*** file scope functions **********************************************/

static bool
is_logging_enabled(void)
{
	static bool logging_initialized = FALSE;
	static bool logging_enabled = FALSE;
	char *mc_ini;

	if (!logging_initialized) {
		mc_ini = g_strdup_printf("%s/%s", home_dir, PROFILE_NAME);
		logging_enabled =
		    get_int(mc_ini, "development.enable_logging", FALSE);
		mhl_mem_free(mc_ini);
		logging_initialized = TRUE;
	}
	return logging_enabled;
}

/*** public functions **************************************************/

void
mc_log(const char *fmt, ...)
{
	va_list args;
	FILE *f;
	char *logfilename;

	if (is_logging_enabled()) {
		va_start(args, fmt);
		logfilename = g_strdup_printf("%s/.mc/log", home_dir);
		if ((f = fopen(logfilename, "a")) != NULL) {
			(void)vfprintf(f, fmt, args);
			(void)fclose(f);
		}
		mhl_mem_free(logfilename);
	}
}
