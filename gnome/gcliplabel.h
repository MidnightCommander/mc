#ifndef __GTK_CLIP_LABEL_H__
#define __GTK_CLIP_LABEL_H__


#include <gdk/gdk.h>
#include <gtk/gtklabel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_CLIP_LABEL(obj)          GTK_CHECK_CAST (obj, gtk_clip_label_get_type (), GtkClipLabel)
#define GTK_CLIP_LABEL_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_clip_label_get_type (), GtkClipLabelClass)
#define GTK_IS_CLIP_LABEL(obj)       GTK_CHECK_TYPE (obj, gtk_clip_label_get_type ())


typedef struct _GtkClipLabel       GtkClipLabel;
typedef struct _GtkClipLabelClass  GtkClipLabelClass;

struct _GtkClipLabel
{
  GtkLabel misc;
};

struct _GtkClipLabelClass
{
  GtkLabelClass parent_class;
};


guint      gtk_clip_label_get_type    (void);
GtkWidget* gtk_clip_label_new         (const char        *str);
void       gtk_clip_label_set          (GtkLabel *label,
					const char *str);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CLIP_LABEL_H__ */
