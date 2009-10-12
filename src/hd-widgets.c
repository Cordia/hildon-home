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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-widgets.h"

#define HD_WIDGETS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_WIDGETS, HDWidgetsPrivate))

struct _HDWidgetsPrivate 
{
  gpointer data;
};

G_DEFINE_ABSTRACT_TYPE (HDWidgets, hd_widgets, G_TYPE_OBJECT);

static void
hd_widgets_init (HDWidgets *widgets)
{
  widgets->priv = HD_WIDGETS_GET_PRIVATE (widgets);
}

static void
hd_widgets_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_widgets_parent_class)->dispose (object);
}

static void
hd_widgets_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_widgets_parent_class)->finalize (object);
}

static void
hd_widgets_class_init (HDWidgetsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_widgets_dispose;
  object_class->finalize = hd_widgets_finalize;

  g_type_class_add_private (klass, sizeof (HDWidgetsPrivate));
}

const gchar *
hd_widgets_get_dialog_title (HDWidgets *widgets)
{
  g_return_val_if_fail (HD_IS_WIDGETS (widgets), NULL);
  g_return_val_if_fail (HD_WIDGETS_GET_CLASS (widgets)->get_dialog_title, NULL);

  return HD_WIDGETS_GET_CLASS (widgets)->get_dialog_title (widgets);
}

GtkTreeModel *
hd_widgets_get_model (HDWidgets *widgets)
{
  g_return_val_if_fail (HD_IS_WIDGETS (widgets), NULL);
  g_return_val_if_fail (HD_WIDGETS_GET_CLASS (widgets)->get_model, NULL);

  return HD_WIDGETS_GET_CLASS (widgets)->get_model (widgets);
}

void
hd_widgets_setup_column_renderers (HDWidgets     *widgets,
                                   GtkCellLayout *column)
{
  g_return_if_fail (HD_IS_WIDGETS (widgets));
  g_return_if_fail (GTK_IS_CELL_LAYOUT (column));
  g_return_if_fail (HD_WIDGETS_GET_CLASS (widgets)->setup_column_renderes);

  HD_WIDGETS_GET_CLASS (widgets)->setup_column_renderes (widgets,
                                                         column);
}

void
hd_widgets_install_widget (HDWidgets   *widgets,
                           GtkTreePath *path)
{
  g_return_if_fail (HD_IS_WIDGETS (widgets));
  g_return_if_fail (path != NULL);
  g_return_if_fail (HD_WIDGETS_GET_CLASS (widgets)->install_widget);

  HD_WIDGETS_GET_CLASS (widgets)->install_widget (widgets,
                                                  path);
}
