/* Determination whether a file system is local or remote.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

// Specification.
#include "filesystem-remote.h"

#include <string.h>

#define streq(s1, s2) (strcmp (s1, s2) == 0)

int
is_remote_fs_type_name (const char *fs_type)
{
    // clang-format off
    return (streq (fs_type, "acfs")
            || streq (fs_type, "afs")
            || streq (fs_type, "autofs")
            || streq (fs_type, "auristorfs")
            || streq (fs_type, "cachefs")
            || streq (fs_type, "ceph")
            || streq (fs_type, "cifs")
            || streq (fs_type, "coda")
            || streq (fs_type, "fhgfs")
            || streq (fs_type, "gfs")
            || streq (fs_type, "gfs2")
            || streq (fs_type, "gpfs")
            || streq (fs_type, "ibrix")
            || streq (fs_type, "lustre")
            || streq (fs_type, "ncpfs")
            || streq (fs_type, "netfs")
            || streq (fs_type, "nfs")
            || streq (fs_type, "nfs3")
            || streq (fs_type, "nfs4")
            || streq (fs_type, "ocfs2")
            || streq (fs_type, "panfs")
            || streq (fs_type, "smb")
            || streq (fs_type, "smb2")
            || streq (fs_type, "smb3")
            || streq (fs_type, "smbfs")
            || streq (fs_type, "snfs")
            || streq (fs_type, "stnfs")
            || streq (fs_type, "userlandfs")
            || streq (fs_type, "vxfs")
            || streq (fs_type, "websearchfs"));
    // clang-format on
}
