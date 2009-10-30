
/** \file logging.h
 *  \brief Header: provides a log file to ease tracing the program
 */

#ifndef MC_LOGGING_H
#define MC_LOGGING_H

/*
   This file provides an easy-to-use function for writing all kinds of
   events into a central log file that can be used for debugging.
 */

#define mc_log_mark() mc_log("%s:%d\n",__FILE,__LINE__)

extern void mc_log(const char *, ...)
	__attribute__((__format__(__printf__,1,2)));

#endif
