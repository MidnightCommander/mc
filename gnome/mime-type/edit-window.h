/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#include "mime-data.h"
#ifndef _EDIT_WINDOW_H_
#define _EDIT_WINDOW_H_


void launch_edit_window (MimeInfo *mi);
void initialize_main_win_vals (void);
void hide_edit_window (void);
void show_edit_window (void);

#endif
