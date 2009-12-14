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

#ifndef __HD_THEME_BACKGROUND_H__
#define __HD_THEME_BACKGROUND_H__

#include "hd-imageset-background.h"

G_BEGIN_DECLS

#define HD_TYPE_THEME_BACKGROUND            (hd_theme_background_get_type ())
#define HD_THEME_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_THEME_BACKGROUND, HDThemeBackground))
#define HD_THEME_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_THEME_BACKGROUND, HDThemeBackgroundClass))
#define HD_IS_THEME_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_THEME_BACKGROUND))
#define HD_IS_THEME_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_THEME_BACKGROUND))
#define HD_THEME_BACKGROUND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_THEME_BACKGROUND, HDThemeBackgroundClass))

typedef struct _HDThemeBackground        HDThemeBackground;
typedef struct _HDThemeBackgroundClass   HDThemeBackgroundClass;
typedef struct _HDThemeBackgroundPrivate HDThemeBackgroundPrivate;

struct _HDThemeBackground
{
  HDImagesetBackground parent;

  HDThemeBackgroundPrivate *priv;
};

struct _HDThemeBackgroundClass
{
  HDImagesetBackgroundClass parent;
};

GType         hd_theme_background_get_type      (void);

HDBackground *hd_theme_background_new           (GFile *desktop_file,
                                                 GFile *image_file);

void          hd_theme_background_get_available (HDAvailableBackgrounds *backgrounds);

G_END_DECLS

#endif

