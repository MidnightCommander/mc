/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#ifndef _MIME_INFO_H_
#define _MIME_INFO_H_
#include "gnome.h"
#include <regex.h>
/* Typedefs */
void init_mime_info (void);
void discard_key_info (void);
void set_mime_key_value (gchar *mime_type, gchar *key, gchar *value);
const char * local_mime_get_value (const char *mime_type, char *key);
void write_user_keys (void);
void write_initial_keys (void);
void remove_mime_info (gchar *mime_type);
#endif
