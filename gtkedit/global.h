#ifndef __CW_GLOBAL_H
#define __CW_GLOBAL_H

/* Some X servers use a different mask for the Alt key: */
#define MyAltMask Mod1Mask
/* #define MyAltMask Mod2Mask */
/* #define MyAltMask Mod3Mask */
/* #define MyAltMask Mod4Mask */
/* #define MyAltMask Mod5Mask */

/* Some servers don't have a right Control key: */
#define MyComposeKey XK_Control_R
/* #define MyComposeKey XK_Shift_L */
/* #define MyComposeKey XK_Shift_R */
/* #define MyComposeKey XK_Meta_L */
/* #define MyComposeKey XK_Meta_R */
/* #define MyComposeKey XK_Alt_L */
/* #define MyComposeKey XK_Alt_R */
/* #define MyComposeKey XK_Super_L */
/* #define MyComposeKey XK_Super_R */
/* #define MyComposeKey XK_Hyper_L */
/* #define MyComposeKey XK_Hyper_R */


/* u_32bit_t should be four bytes */
#define u_32bit_t unsigned int
#define word unsigned short
#define byte unsigned char

#endif				/* __CW_GLOBAL_H */

