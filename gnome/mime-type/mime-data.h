/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#ifndef _MIME_DATA_H_
#define _MIME_DATA_H_
#include "gnome.h"
#include <regex.h>
/* Typedefs */
typedef struct {
	char     *mime_type;
	regex_t  *regex[2];
	GList    *ext[2];
        char     *ext_readable[2];
        char     *regex_readable[2];
	char     *file_name;
        GList    *keys;
} MimeInfo;

GtkWidget *get_mime_clist (void);
void init_mime_type (void);
void add_clicked (GtkWidget *widget, gpointer data);

#endif
