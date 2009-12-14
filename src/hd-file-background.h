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

#ifndef __HD_FILE_BACKGROUND_H__
#define __HD_FILE_BACKGROUND_H__

#include <gio/gio.h>

#include "hd-background.h"
#include "hd-available-backgrounds.h"

G_BEGIN_DECLS

#define HD_TYPE_FILE_BACKGROUND            (hd_file_background_get_type ())
#define HD_FILE_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_FILE_BACKGROUND, HDFileBackground))
#define HD_FILE_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_FILE_BACKGROUND, HDFileBackgroundClass))
#define HD_IS_FILE_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_FILE_BACKGROUND))
#define HD_IS_FILE_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_FILE_BACKGROUND))
#define HD_FILE_BACKGROUND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_FILE_BACKGROUND, HDFileBackgroundClass))

typedef struct _HDFileBackground        HDFileBackground;
typedef struct _HDFileBackgroundClass   HDFileBackgroundClass;
typedef struct _HDFileBackgroundPrivate HDFileBackgroundPrivate;

struct _HDFileBackground
{
  HDBackground parent;

  HDFileBackgroundPrivate *priv;
};

struct _HDFileBackgroundClass
{
  HDBackgroundClass parent;
};

GType         hd_file_background_get_type     (void);

HDBackground *hd_file_background_new          (GFile *image_file);

void          hd_file_background_add_to_store (HDFileBackground       *background,
                                               HDAvailableBackgrounds *backgrounds);

char         *hd_file_background_get_label      (HDFileBackground *background);
GFile        *hd_file_background_get_image_file (HDFileBackground *background);

G_END_DECLS

#endif

