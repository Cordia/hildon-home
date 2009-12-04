/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
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

#ifndef __HD_BACKGROUND_H__
#define __HD_BACKGROUND_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HD_TYPE_BACKGROUND            (hd_background_get_type ())
#define HD_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BACKGROUND, HDBackground))
#define HD_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_BACKGROUND, HDBackgroundClass))
#define HD_IS_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BACKGROUND))
#define HD_IS_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_BACKGROUND))
#define HD_BACKGROUND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_BACKGROUND, HDBackgroundClass))

typedef struct _HDBackground        HDBackground;
typedef struct _HDBackgroundClass   HDBackgroundClass;
typedef struct _HDBackgroundPrivate HDBackgroundPrivate;

enum
{
  HD_BACKGROUND_COL_LABEL = 0,
  HD_BACKGROUND_COL_THUMBNAIL = 9,
  HD_BACKGROUND_COL_OBJECT = 10,
  HD_BACKGROUND_NUM_COLS
};

struct _HDBackground
{
  GObject parent;

  HDBackgroundPrivate *priv;
};

struct _HDBackgroundClass
{
  GObjectClass parent;

  void (*set_for_current_view) (HDBackground   *background,
                                guint           view,
                                GCancellable   *cancellable);
};

GType hd_background_get_type      (void);

void  hd_background_set_thumbnail_from_file (HDBackground *background,
                                             GtkTreeModel *model,
                                             GtkTreeIter  *iter,
                                             GFile        *file);

void hd_background_set_for_current_view (HDBackground   *background,
                                         guint           view,
                                         GCancellable   *cancellable);

G_END_DECLS

#endif

