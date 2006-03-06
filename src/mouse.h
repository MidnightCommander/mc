#ifndef MC_MOUSE_H
#define MC_MOUSE_H

#ifdef HAVE_LIBGPM

/* GPM mouse support include file */
#include <gpm.h>

#else

/* Equivalent definitions for non-GPM mouse support */
/* These lines are modified version from the lines appearing in the */
/* gpm.h include file of the Linux General Purpose Mouse server */

#define GPM_B_LEFT	(1 << 2)
#define GPM_B_MIDDLE    (1 << 1)
#define GPM_B_RIGHT     (1 << 0)

/* Xterm mouse support supports only GPM_DOWN and GPM_UP */
/* If you use others make sure your code also works without them */
enum Gpm_Etype {
  GPM_MOVE=1,
  GPM_DRAG=2,   /* exactly one in four is active at a time */
  GPM_DOWN=4,
  GPM_UP=  8,

#define GPM_BARE_EVENTS(ev) ((ev)&0xF)

  GPM_SINGLE=16,            /* at most one in three is set */
  GPM_DOUBLE=32,
  GPM_TRIPLE=64,

  GPM_MFLAG=128,            /* motion during click? */
  GPM_HARD=256             /* if set in the defaultMask, force an already
                              used event to pass over to another handler */
};

typedef struct Gpm_Event {
  int buttons, x, y;
  enum Gpm_Etype type;
} Gpm_Event;

#endif /* !HAVE_LIBGPM */

/* General (i.e. both for xterm and gpm) mouse support definitions */

/* Constants returned from the mouse callback */
enum { MOU_NORMAL, MOU_REPEAT };

/* Mouse callback */
typedef int (*mouse_h) (Gpm_Event *, void *);

/* Type of mouse support */
typedef enum {
    MOUSE_NONE,		/* Not detected yet */
    MOUSE_DISABLED,	/* Explicitly disabled by -d */
    MOUSE_GPM,		/* Support using GPM on Linux */
    MOUSE_XTERM,	/* Support using xterm-style mouse reporting */
    MOUSE_XTERM_NORMAL_TRACKING = MOUSE_XTERM,
    MOUSE_XTERM_BUTTON_EVENT_TRACKING
} Mouse_Type;

/* Type of the currently used mouse */
extern Mouse_Type use_mouse_p;

/* The mouse is currently: 1 - enabled, 0 - disabled */
extern int mouse_enabled;

/* String indicating that a mouse event has occured, usually "\E[M" */
extern const char *xmouse_seq;

void init_mouse (void);
void enable_mouse (void);
void disable_mouse (void);

/* Mouse wheel events */
#ifndef GPM_B_DOWN
#define GPM_B_DOWN	(1 << 5)
#endif

#ifndef GPM_B_UP
#define GPM_B_UP	(1 << 4)
#endif

#ifdef HAVE_LIBGPM

/* GPM specific mouse support definitions */
void show_mouse_pointer (int x, int y);

#else

/* Mouse support definitions for non-GPM mouse */
#define show_mouse_pointer(a,b)

#endif

#endif
