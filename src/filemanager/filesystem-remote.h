/* Determination whether a file system is local or remote.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

#ifndef _FILESYSTEM_REMOTE_H
#define _FILESYSTEM_REMOTE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // clang-format off
/* Returns 1 if the filesystem type name FS_TYPE is known to be a
   remote filesystem type, 0 otherwise.  */
extern int is_remote_fs_type_name (const char *fs_type);
    // clang-format on

#ifdef __cplusplus
}
#endif

#endif /* _FILESYSTEM_REMOTE_H */
