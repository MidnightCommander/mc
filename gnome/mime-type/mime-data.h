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
        GList    *user_ext[2];
        char     *ext_readable[2];
        char     *regex_readable[2];
	char     *file_name;
        GList    *keys;
} MimeInfo;

extern GHashTable *user_mime_types;
extern void add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user);

GtkWidget *get_mime_clist (void);
void init_mime_type (void);
void delete_clicked (GtkWidget *widget, gpointer data);
void add_clicked (GtkWidget *widget, gpointer data);
void edit_clicked (GtkWidget *widget, gpointer data);
void add_new_mime_type (gchar *mime_type, gchar *ext, gchar *regexp1, gchar *regexp2);
void write_user_mime (void);
void write_initial_mime (void);
void reread_list (void);
void discard_mime_info (void);
#endif
