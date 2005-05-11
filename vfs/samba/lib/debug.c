/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

/* -------------------------------------------------------------------------- **
 * Defines...
 *
 *  FORMAT_BUFR_MAX - Index of the last byte of the format buffer;
 *                    format_bufr[FORMAT_BUFR_MAX] should always be reserved
 *                    for a terminating nul byte.
 */

#define FORMAT_BUFR_MAX ( sizeof( format_bufr ) - 1 )

/* -------------------------------------------------------------------------- **
 * This module implements Samba's debugging utility.
 *
 * The syntax of a debugging log file is represented as:
 *
 *  <debugfile> :== { <debugmsg> }
 *
 *  <debugmsg>  :== <debughdr> '\n' <debugtext>
 *
 *  <debughdr>  :== '[' TIME ',' LEVEL ']' [ [FILENAME ':'] [FUNCTION '()'] ]
 *
 *  <debugtext> :== { <debugline> }
 *
 *  <debugline> :== TEXT '\n'
 *
 * TEXT     is a string of characters excluding the newline character.
 * LEVEL    is the DEBUG level of the message (an integer in the range 0..10).
 * TIME     is a timestamp.
 * FILENAME is the name of the file from which the debug message was generated.
 * FUNCTION is the function from which the debug message was generated.
 *
 * Basically, what that all means is:
 *
 * - A debugging log file is made up of debug messages.
 *
 * - Each debug message is made up of a header and text.  The header is
 *   separated from the text by a newline.
 *
 * - The header begins with the timestamp and debug level of the message
 *   enclosed in brackets.  The filename and function from which the
 *   message was generated may follow.  The filename is terminated by a
 *   colon, and the function name is terminated by parenthesis.
 *
 * - The message text is made up of zero or more lines, each terminated by
 *   a newline.
 */

/* -------------------------------------------------------------------------- **
 * External variables.
 *
 *  dbf           - Global debug file handle.
 *  debugf        - Debug file name.
 *  append_log    - If True, then the output file will be opened in append
 *                  mode.
 *  DEBUGLEVEL    - System-wide debug message limit.  Messages with message-
 *                  levels higher than DEBUGLEVEL will not be processed.
 */

FILE   *dbf        = NULL;
pstring debugf     = "";
BOOL    append_log = False;
int     DEBUGLEVEL = 1;


/* -------------------------------------------------------------------------- **
 * Internal variables.
 *
 *  stdout_logging  - Default False, if set to True then dbf will be set to
 *                    stdout and debug output will go to dbf only, and not
 *                    to syslog.  Set in setup_logging() and read in Debug1().
 *
 *  syslog_level    - Internal copy of the message debug level.  Written by
 *                    dbghdr() and read by Debug1().
 *
 *  format_bufr     - Used to format debug messages.  The dbgtext() function
 *                    prints debug messages to a string, and then passes the
 *                    string to format_debug_text(), which uses format_bufr
 *                    to build the formatted output.
 *
 *  format_pos      - Marks the first free byte of the format_bufr.
 */

static BOOL    stdout_logging = False;
static pstring format_bufr    = { '\0' };
static size_t  format_pos     = 0;


/* -------------------------------------------------------------------------- **
 * Functions...
 */

/* ************************************************************************** **
 * get ready for syslog stuff
 * ************************************************************************** **
 */
void setup_logging( char *pname, BOOL interactive )
  {
  if( interactive )
    {
    stdout_logging = True;
    dbf = stderr;
    }
  } /* setup_logging */

/* ************************************************************************** **
 * Write an debug message on the debugfile.
 * This is called by dbghdr() and format_debug_text().
 * ************************************************************************** **
 */
#ifdef HAVE_STDARG_H
 int Debug1( char *format_str, ... )
{
#else
 int Debug1(va_alist)
va_dcl
{  
  char *format_str;
#endif
  va_list ap;  
  int old_errno = errno;

  if( stdout_logging )
    {
#ifdef HAVE_STDARG_H
    va_start( ap, format_str );
#else
    va_start( ap );
    format_str = va_arg( ap, char * );
#endif
    (void)vfprintf( dbf, format_str, ap );
    va_end( ap );
    errno = old_errno;
    return( 0 );
    }
  
    if( !dbf && *debugf)
      {
      mode_t oldumask = umask( 022 );

      if( append_log )
        dbf = sys_fopen( debugf, "a" );
      else
        dbf = sys_fopen( debugf, "w" );
      (void)umask( oldumask );
      if( dbf )
        {
        setbuf( dbf, NULL );
        }
      else
        {
        errno = old_errno;
        return(0);
        }
      }

    if (dbf)
    {
#ifdef HAVE_STDARG_H
    va_start( ap, format_str );
#else
    va_start( ap );
    format_str = va_arg( ap, char * );
#endif
    (void)vfprintf( dbf, format_str, ap );
    va_end( ap );
    (void)fflush( dbf );
    }

  errno = old_errno;

  return( 0 );
  } /* Debug1 */


/* ************************************************************************** **
 * Print the buffer content via Debug1(), then reset the buffer.
 *
 *  Input:  none
 *  Output: none
 *
 * ************************************************************************** **
 */
static void bufr_print( void )
  {
  format_bufr[format_pos] = '\0';
  (void)Debug1( "%s", format_bufr );
  format_pos = 0;
  } /* bufr_print */

/* ************************************************************************** **
 * Format the debug message text.
 *
 *  Input:  msg - Text to be added to the "current" debug message text.
 *
 *  Output: none.
 *
 *  Notes:  The purpose of this is two-fold.  First, each call to syslog()
 *          (used by Debug1(), see above) generates a new line of syslog
 *          output.  This is fixed by storing the partial lines until the
 *          newline character is encountered.  Second, printing the debug
 *          message lines when a newline is encountered allows us to add
 *          spaces, thus indenting the body of the message and making it
 *          more readable.
 *
 * ************************************************************************** **
 */
static void format_debug_text( char *msg )
  {
  size_t i;
  BOOL timestamp = (!stdout_logging && (lp_timestamp_logs() || 
					!(lp_loaded())));

  for( i = 0; msg[i]; i++ )
    {
    /* Indent two spaces at each new line. */
    if(timestamp && 0 == format_pos)
      {
      format_bufr[0] = format_bufr[1] = ' ';
      format_pos = 2;
      }

    /* If there's room, copy the character to the format buffer. */
    if( format_pos < FORMAT_BUFR_MAX )
      format_bufr[format_pos++] = msg[i];

    /* If a newline is encountered, print & restart. */
    if( '\n' == msg[i] )
      bufr_print();

    /* If the buffer is full dump it out, reset it, and put out a line
     * continuation indicator.
     */
    if( format_pos >= FORMAT_BUFR_MAX )
      {
      bufr_print();
      (void)Debug1( " +>\n" );
      }
    }

  /* Just to be safe... */
  format_bufr[format_pos] = '\0';
  } /* format_debug_text */

/* ************************************************************************** **
 * Flush debug output, including the format buffer content.
 *
 *  Input:  none
 *  Output: none
 *
 * ************************************************************************** **
 */
void dbgflush( void )
  {
  bufr_print();
  (void)fflush( dbf );
  } /* dbgflush */

/* ************************************************************************** **
 * Print a Debug Header.
 *
 *  Input:  level - Debug level of the message (not the system-wide debug
 *                  level.
 *          file  - Pointer to a string containing the name of the file
 *                  from which this function was called, or an empty string
 *                  if the __FILE__ macro is not implemented.
 *          func  - Pointer to a string containing the name of the function
 *                  from which this function was called, or an empty string
 *                  if the __FUNCTION__ macro is not implemented.
 *          line  - line number of the call to dbghdr, assuming __LINE__
 *                  works.
 *
 *  Output: Always True.  This makes it easy to fudge a call to dbghdr()
 *          in a macro, since the function can be called as part of a test.
 *          Eg: ( (level <= DEBUGLEVEL) && (dbghdr(level,"",line)) )
 *
 *  Notes:  This function takes care of setting syslog_level.
 *
 * ************************************************************************** **
 */
BOOL dbghdr( int level, const char *file, const char *func, int line )
  {
  if( format_pos )
    {
    /* This is a fudge.  If there is stuff sitting in the format_bufr, then
     * the *right* thing to do is to call
     *   format_debug_text( "\n" );
     * to write the remainder, and then proceed with the new header.
     * Unfortunately, there are several places in the code at which
     * the DEBUG() macro is used to build partial lines.  That in mind,
     * we'll work under the assumption that an incomplete line indicates
     * that a new header is *not* desired.
     */
    return( True );
    }

  /* Don't print a header if we're logging to stdout. */
  if( stdout_logging )
    return( True );

  /* Print the header if timestamps are turned on.  If parameters are
   * not yet loaded, then default to timestamps on.
   */
  if( lp_timestamp_logs() || !(lp_loaded()) )
    {
    /* Print it all out at once to prevent split syslog output. */
    (void)Debug1( "[%s, %d] %s:%s(%d)\n",
                  timestring(), level, file, func, line );
    }

  return( True );
  } /* dbghdr */

/* ************************************************************************** **
 * Add text to the body of the "current" debug message via the format buffer.
 *
 *  Input:  format_str  - Format string, as used in printf(), et. al.
 *          ...         - Variable argument list.
 *
 *  ..or..  va_alist    - Old style variable parameter list starting point.
 *
 *  Output: Always True.  See dbghdr() for more info, though this is not
 *          likely to be used in the same way.
 *
 * ************************************************************************** **
 */
#ifdef HAVE_STDARG_H
 BOOL dbgtext( char *format_str, ... )
  {
  va_list ap;
  pstring msgbuf;

  va_start( ap, format_str ); 
  vslprintf( msgbuf, sizeof(msgbuf)-1, format_str, ap );
  va_end( ap );

  format_debug_text( msgbuf );

  return( True );
  } /* dbgtext */

#else
 BOOL dbgtext( va_alist )
 va_dcl
  {
  char *format_str;
  va_list ap;
  pstring msgbuf;

  va_start( ap );
  format_str = va_arg( ap, char * );
  vslprintf( msgbuf, sizeof(msgbuf)-1, format_str, ap );
  va_end( ap );

  format_debug_text( msgbuf );

  return( True );
  } /* dbgtext */

#endif

/* ************************************************************************** */
