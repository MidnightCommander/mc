/* This program should be setuid vcsa and /dev/vcsa* should be
   owned by the same user too.
   Partly rewritten by Jakub Jelinek <jakub@redhat.com>.  */

/* General purpose Linux console screen save/restore server
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.
   Original idea from Unix Interactive Tools version 3.2b (tty.c)
   This code requires root privileges.
   You may want to make the cons.saver setuid root.
   The code should be safe even if it is setuid but who knows?
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* This code does _not_ need to be setuid root. However, it needs
   read/write access to /dev/vcsa* (which is priviledged
   operation). You should create user vcsa, make cons.saver setuid
   user vcsa, and make all vcsa's owned by user vcsa.

   Seeing other peoples consoles is bad thing, but believe me, full
   root is even worse. */

/** \file cons.saver.c
 *  \brief Source: general purpose Linux console screen save/restore server
 *
 *  This code does _not_ need to be setuid root. However, it needs
 *  read/write access to /dev/vcsa* (which is priviledged
 *  operation). You should create user vcsa, make cons.saver setuid
 *  user vcsa, and make all vcsa's owned by user vcsa.
 *  Seeing other peoples consoles is bad thing, but believe me, full
 *  root is even worse.
 */

#include <config.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>

#define LINUX_CONS_SAVER_C
#include "cons.saver.h"
#include "../src/tty/win.h"

static void
send_contents (char *buffer, unsigned int columns, unsigned int rows)
{
  unsigned char begin_line = 0, end_line = 0;
  unsigned int lastline, index, x;
  unsigned char message, outbuf[1024], *p;
  unsigned short bytes;

  index = 2 * rows * columns;
  for (lastline = rows; lastline > 0; lastline--)
    for (x = 0; x < columns; x++)
      {
	index -= 2;
	if (buffer [index] != ' ')
	  goto out;
      }
out:

  message = CONSOLE_CONTENTS;
  write (1, &message, 1);

  read (0, &begin_line, 1);
  read (0, &end_line, 1);
  if (begin_line > lastline)
    begin_line = lastline;
  if (end_line > lastline)
    end_line = lastline;

  index = (end_line - begin_line) * columns;
  bytes = index;
  if (index != bytes)
    bytes = 0;
  write (1, &bytes, 2);
  if (! bytes)
    return;

  p = outbuf;
  for (index = 2 * begin_line * columns;
       index < 2 * end_line * columns;
       index += 2)
    {
      *p++ = buffer [index];
      if (p == outbuf + sizeof (outbuf))
	{
	  write (1, outbuf, sizeof (outbuf));
	  p = outbuf;
	}
    }

  if (p != outbuf)
    write (1, outbuf, p - outbuf);
}

static void __attribute__ ((noreturn))
die (void)
{
  unsigned char zero = 0;
  write (1, &zero, 1);
  exit (3);
}

int
main (int argc, char **argv)
{
  unsigned char action = 0, console_flag = 3;
  int console_fd, vcsa_fd, console_minor, buffer_size;
  struct stat st;
  uid_t uid, euid;
  char *buffer, *tty_name, console_name [16], vcsa_name [16];
  const char *p, *q;
  struct winsize winsz;

  close (2);

  if (argc != 2)
    die ();

  tty_name = argv [1];
  if (strnlen (tty_name, 15) == 15
      || strncmp (tty_name, "/dev/", 5))
    die ();

  setsid ();
  uid = getuid ();
  euid = geteuid ();

  if (seteuid (uid) < 0)
    die ();
  console_fd = open (tty_name, O_RDONLY);
  if (console_fd < 0)
    die ();
  if (fstat (console_fd, &st) < 0 || ! S_ISCHR (st.st_mode))
    die ();
  if ((st.st_rdev & 0xff00) != 0x0400)
    die ();
  console_minor = (int) (st.st_rdev & 0x00ff);
  if (console_minor < 1 || console_minor > 63)
    die ();
  if (st.st_uid != uid)
    die ();

  switch (tty_name [5])
    {
    /* devfs */
    case 'v': p = "/dev/vc/%d"; q = "/dev/vcc/a%d"; break;
    /* /dev/ttyN */
    case 't': p = "/dev/tty%d"; q = "/dev/vcsa%d"; break;
    default: die (); break;
    }

  snprintf (console_name, sizeof (console_name), p, console_minor);
  if (strncmp (console_name, tty_name, sizeof (console_name)) != 0)
    die ();

  if (seteuid (euid) < 0)
    die ();

  snprintf (vcsa_name, sizeof (vcsa_name), q, console_minor);
  vcsa_fd = open (vcsa_name, O_RDWR);
  if (vcsa_fd < 0)
    die ();
  if (fstat (vcsa_fd, &st) < 0 || ! S_ISCHR (st.st_mode))
    die ();

  if (seteuid (uid) < 0)
    die ();

  winsz.ws_col = winsz.ws_row = 0;
  if (ioctl (console_fd, TIOCGWINSZ, &winsz) < 0
      || winsz.ws_col <= 0 || winsz.ws_row <= 0
      || winsz.ws_col >= 256 || winsz.ws_row >= 256)
    die ();

  buffer_size = 4 + 2 * winsz.ws_col * winsz.ws_row;
  buffer = calloc (buffer_size, 1);
  if (buffer == NULL)
    die ();

  write (1, &console_flag, 1);

  while (console_flag && read (0, &action, 1) == 1)
    {
      switch (action)
	{
	case CONSOLE_DONE:
	  console_flag = 0;
	  continue;
	case CONSOLE_SAVE:
	  if (seteuid (euid) < 0
	      || lseek (vcsa_fd, 0, 0) != 0
	      || fstat (console_fd, &st) < 0 || st.st_uid != uid
	      || read (vcsa_fd, buffer, buffer_size) != buffer_size
	      || fstat (console_fd, &st) < 0 || st.st_uid != uid)
	    memset (buffer, 0, buffer_size);
	  if (seteuid (uid) < 0)
	    die ();
	  break;
	case CONSOLE_RESTORE:
	  if (seteuid (euid) >= 0
	      && lseek (vcsa_fd, 0, 0) == 0
	      && fstat (console_fd, &st) >= 0 && st.st_uid == uid)
	    write (vcsa_fd, buffer, buffer_size);
	  if (seteuid (uid) < 0)
	    die ();
	  break;
	case CONSOLE_CONTENTS:
	  send_contents (buffer + 4, winsz.ws_col, winsz.ws_row);
	  break;
	}

      write (1, &console_flag, 1);
    }

  exit (0);
}
