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

#ifndef __HD_IMAGESET_BACKGROUND_H__
#define __HD_IMAGESET_BACKGROUND_H__

#include <gio/gio.h>

#include "hd-background.h"
#include "hd-available-backgrounds.h"

G_BEGIN_DECLS

#define HD_TYPE_IMAGESET_BACKGROUND            (hd_imageset_background_get_type ())
#define HD_IMAGESET_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_IMAGESET_BACKGROUND, HDImagesetBackground))
#define HD_IMAGESET_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_IMAGESET_BACKGROUND, HDImagesetBackgroundClass))
#define HD_IS_IMAGESET_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_IMAGESET_BACKGROUND))
#define HD_IS_IMAGESET_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_IMAGESET_BACKGROUND))
#define HD_IMAGESET_BACKGROUND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_IMAGESET_BACKGROUND, HDImagesetBackgroundClass))

typedef struct _HDImagesetBackground        HDImagesetBackground;
typedef struct _HDImagesetBackgroundClass   HDImagesetBackgroundClass;
typedef struct _HDImagesetBackgroundPrivate HDImagesetBackgroundPrivate;

struct _HDImagesetBackground
{
  HDBackground parent;

  HDImagesetBackgroundPrivate *priv;
};

struct _HDImagesetBackgroundClass
{
  HDBackgroundClass parent;
};

GType         hd_imageset_background_get_type      (void);

HDBackground *hd_imageset_background_new           (GFile *file);

void          hd_imageset_background_init_async    (HDImagesetBackground  *background,
                                                    GCancellable          *cancellable,
                                                    GAsyncReadyCallback    callback,
                                                    gpointer               user_data);
gboolean      hd_imageset_background_init_finish   (HDImagesetBackground  *background,
                                                    GAsyncResult          *result,
                                                    GError               **error);

void          hd_imageset_background_get_available (HDAvailableBackgrounds *backgrounds);

G_END_DECLS

#endif

