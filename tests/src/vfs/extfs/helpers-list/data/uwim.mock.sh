#!/bin/sh

if [ "$1" = "dir" ]; then
    cat "${MC_TEST_EXTFS_INPUT}"
elif [ "$1" = "info" ]; then
    cat << EOF
WIM Information:
----------------
Path:           data/uwim.source
GUID:           0x5185f5074896d9924af9b7afb4087417
Version:        68864
Image Count:    1
Compression:    LZX
Chunk Size:     32768 bytes
Part Number:    1/1
Boot Index:     0
Size:           107618 bytes
Attributes:     Relative path junction

Available Images:
-----------------
Index:                  1
Name:                   mchelp
Description:            help pages
Directory Count:        7
File Count:             15
Total Bytes:            316021
Hard Link Bytes:        0
Creation Time:          Sun Nov 02 02:10:00 2025 UTC
Last Modification Time: Sun Nov 02 02:10:00 2025 UTC
WIMBoot compatible:     no
EOF
fi
