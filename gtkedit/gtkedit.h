/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_EDIT_H__
#define __GTK_EDIT_H__


#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkeditable.h>
#include <gtkedit/mousemark.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_EDIT(obj)          GTK_CHECK_CAST (obj, gtk_edit_get_type (), GtkEdit)
#define GTK_EDIT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_edit_get_type (), GtkEditClass)
#define GTK_IS_EDIT(obj)       GTK_CHECK_TYPE (obj, gtk_edit_get_type ())


typedef struct _GtkEdit           GtkEdit;
typedef struct _GtkEditClass      GtkEditClass;


struct _GtkEdit
{
  GtkEditable editable;

  GdkWindow *text_area;
  GtkWidget *menubar, *status;

  GtkAdjustment *hadj;
  gint last_hadj_value;

  GtkAdjustment *vadj;
  gint last_vadj_value;

  GdkGC *gc;
  GdkColor color[256];
  int color_last_pixel;
  void (*destroy_me) (void *);
  void *destroy_me_user_data;
  struct editor_widget *editor;
  gulong options;
  gint timer;
};

struct _GtkEditClass
{
  GtkEditableClass parent_class;
};


guint      gtk_edit_get_type        (void);
GtkWidget* gtk_edit_new             (GtkAdjustment *hadj,
				     GtkAdjustment *vadj);
void       gtk_edit_set_editable    (GtkEdit       *text,
				     gint           editable);
void       gtk_edit_set_word_wrap   (GtkEdit       *text,
				     gint           word_wrap);
void       gtk_edit_set_adjustments (GtkEdit       *text,
				     GtkAdjustment *hadj,
				     GtkAdjustment *vadj);
void       gtk_edit_set_point       (GtkEdit       *text,
				     guint          index);
guint      gtk_edit_get_point       (GtkEdit       *text);
guint      gtk_edit_get_length      (GtkEdit       *text);
void       gtk_edit_freeze          (GtkEdit       *text);
void       gtk_edit_thaw            (GtkEdit       *text);
void       gtk_edit_insert          (GtkEdit       *text,
				     GdkFont       *font,
				     GdkColor      *fore,
				     GdkColor      *back,
				     const char    *chars,
				     gint           length);
gint       gtk_edit_backward_delete (GtkEdit       *text,
				     guint          nchars);
gint       gtk_edit_forward_delete  (GtkEdit       *text,
				     guint          nchars);


char *gtk_edit_dialog_get_load_file (guchar * directory, guchar * file, guchar * heading);
char *gtk_edit_dialog_get_save_file (guchar * directory, guchar * file, guchar * heading);
void gtk_edit_dialog_error (guchar * heading, char *fmt,...);
void gtk_edit_dialog_message (guchar * heading, char *fmt,...);
int gtk_edit_dialog_query (guchar * heading, guchar * first,...);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_EDIT_H__ */
