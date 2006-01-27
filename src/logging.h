#ifndef MC_LOGGING_H
#define MC_LOGGING_H

/*
   This file provides an easy-to-use function for writing all kinds of
   events into a central log file that can be used for debugging.
 */

extern void mc_log(const char *, ...)
	__attribute__((__format__(__printf__,1,2)));

#endif
