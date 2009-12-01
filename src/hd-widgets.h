/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __HD_WIDGETS_H__
#define __HD_WIDGETS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HD_TYPE_WIDGETS            (hd_widgets_get_type ())
#define HD_WIDGETS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_WIDGETS, HDWidgets))
#define HD_WIDGETS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_WIDGETS, HDWidgetsClass))
#define HD_IS_WIDGETS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_WIDGETS))
#define HD_IS_WIDGETS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_WIDGETS))
#define HD_WIDGETS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_WIDGETS, HDWidgetsClass))

typedef struct _HDWidgets        HDWidgets;
typedef struct _HDWidgetsClass   HDWidgetsClass;
typedef struct _HDWidgetsPrivate HDWidgetsPrivate;

struct _HDWidgets 
{
  GObject gobject;

  HDWidgetsPrivate *priv;
};

struct _HDWidgetsClass 
{
  GObjectClass parent_class;

  const gchar  *(*get_dialog_title)      (HDWidgets     *widgets);
  GtkTreeModel *(*get_model)             (HDWidgets     *widgets);
  void          (*setup_column_renderes) (HDWidgets     *widgets,
                                          GtkCellLayout *colum);
  void          (*install_widget)        (HDWidgets     *widgets,
                                          GtkTreePath   *path);
  gint          (*get_text_column)       (HDWidgets     *widgets);
};

GType         hd_widgets_get_type               (void);

const gchar  *hd_widgets_get_dialog_title       (HDWidgets     *widgets);
GtkTreeModel *hd_widgets_get_model              (HDWidgets     *widgets);
void          hd_widgets_setup_column_renderers (HDWidgets     *widgets,
                                                 GtkCellLayout *column);
void          hd_widgets_install_widget         (HDWidgets     *widgets,
                                                 GtkTreePath   *path);
gint          hd_widgets_get_text_column        (HDWidgets     *widgets);

G_END_DECLS

#endif /* __HD_WIDGETS_H__ */
