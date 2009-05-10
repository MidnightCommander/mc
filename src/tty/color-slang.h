
/** \file color-slang.h
 *  \brief Header: S-Lang-specific color setup
 */

#ifndef MC_COLOR_SLANG_H
#define MC_COLOR_SLANG_H

#define IF_COLOR(co, bw) (use_colors ? COLOR_PAIR (co) : bw)

#define MARKED_SELECTED_COLOR IF_COLOR (4, (SLtt_Use_Ansi_Colors ? A_BOLD_REVERSE : A_REVERSE | A_BOLD))

void mc_init_pair (int index, const char *foreground, const char *background);

#endif				/* MC_COLOR_SLANG_H */
