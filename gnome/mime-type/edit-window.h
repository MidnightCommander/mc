/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#ifndef _EDIT_WINDOW_H_
#define _EDIT_WINDOW_H_

#include "mime-data.h"

void launch_edit_window (MimeInfo *mi);
void initialize_main_win_vals ();
void hide_edit_window ();
void show_edit_window ();
void edit_clicked ();
#endif
