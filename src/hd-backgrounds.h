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

#ifndef __HD_BACKGROUNDS_H__
#define __HD_BACKGROUNDS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_BACKGROUNDS            (hd_backgrounds_get_type ())
#define HD_BACKGROUNDS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BACKGROUNDS, HDBackgrounds))
#define HD_BACKGROUNDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_BACKGROUNDS, HDBackgroundsClass))
#define HD_IS_BACKGROUNDS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BACKGROUNDS))
#define HD_IS_BACKGROUNDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_BACKGROUNDS))
#define HD_BACKGROUNDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_BACKGROUNDS, HDBackgroundsClass))

typedef struct _HDBackgrounds        HDBackgrounds;
typedef struct _HDBackgroundsClass   HDBackgroundsClass;
typedef struct _HDBackgroundsPrivate HDBackgroundsPrivate;

struct _HDBackgrounds 
{
  GObject gobject;

  HDBackgroundsPrivate *priv;
};

struct _HDBackgroundsClass 
{
  GObjectClass parent_class;
};

GType          hd_backgrounds_get_type        (void);

HDBackgrounds *hd_backgrounds_get             (void);

void           hd_backgrounds_startup         (HDBackgrounds *backgrounds);
void           hd_backgrounds_set_background  (HDBackgrounds *backgrounds,
                                               guint          view,
                                               const gchar   *uri,
                                               GSourceFunc    done_callback,
                                               gpointer       cb_data);

const gchar *  hd_backgrounds_get_background  (HDBackgrounds  *backgrounds,
                                               guint           view);

G_END_DECLS

#endif /* __HD_BACKGROUNDS_H__ */
