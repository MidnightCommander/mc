/*
   Simple timer for the Midnight Commander.

   Copyright (C) 2013-2017
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin 2013

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 *  \brief Source: simple timer
 */

#include <config.h>

#include <sys/time.h>

#include "lib/global.h"
#include "lib/timer.h"

/*** global variables ****************************************************************************/

/**
 * mc_timer_t:
 *
 * Opaque datatype that records a start time.
 * #mc_timer_t records a start time, and counts microseconds elapsed since
 * that time.
 **/
struct mc_timer_t
{
    guint64 start;
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Creates a new timer, and starts timing.
 *
 * @return: a new #mc_timer_t.
 **/
mc_timer_t *
mc_timer_new (void)
{
    mc_timer_t *timer;
    struct timeval tv;

    timer = g_new (mc_timer_t, 1);
    gettimeofday (&tv, NULL);
    timer->start = (guint64) tv.tv_sec * G_USEC_PER_SEC + (guint64) tv.tv_usec;

    return timer;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Destroys a timer, freeing associated resources.
 *
 * @timer: an #mc_timer_t to destroy.
 **/
void
mc_timer_destroy (mc_timer_t * timer)
{
    g_free (timer);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Obtains the time since the timer was started.
 *
 * @timer: an #mc_timer_t.
 *
 * @return: microseconds elapsed, the time since the timer was started
 *
 **/
guint64
mc_timer_elapsed (const mc_timer_t * timer)
{
    struct timeval tv;

    gettimeofday (&tv, NULL);

    return ((guint64) tv.tv_sec * G_USEC_PER_SEC + (guint64) tv.tv_usec - timer->start);
}

/* --------------------------------------------------------------------------------------------- */
