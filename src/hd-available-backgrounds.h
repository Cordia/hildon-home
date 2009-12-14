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

#ifndef __HD_AVAILABLE_BACKGROUNDS_H__
#define __HD_AVAILABLE_BACKGROUNDS_H__

#include "hd-background.h"

G_BEGIN_DECLS

#define HD_TYPE_AVAILABLE_BACKGROUNDS             (hd_available_backgrounds_get_type ())
#define HD_AVAILABLE_BACKGROUNDS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_AVAILABLE_BACKGROUNDS, HDAvailableBackgrounds))
#define HD_AVAILABLE_BACKGROUNDS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_AVAILABLE_BACKGROUNDS, HDAvailableBackgroundsClass))
#define HD_IS_AVAILABLE_BACKGROUNDS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_AVAILABLE_BACKGROUNDS))
#define HD_IS_AVAILABLE_BACKGROUNDS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_AVAILABLE_BACKGROUNDS))
#define HD_AVAILABLE_BACKGROUNDS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_AVAILABLE_BACKGROUNDS, HDAvailableBackgroundsClass))

typedef struct _HDAvailableBackgrounds        HDAvailableBackgrounds;
typedef struct _HDAvailableBackgroundsClass   HDAvailableBackgroundsClass;
typedef struct _HDAvailableBackgroundsPrivate HDAvailableBackgroundsPrivate;

struct _HDAvailableBackgrounds
{
  GInitiallyUnowned parent;

  HDAvailableBackgroundsPrivate *priv;
};

struct _HDAvailableBackgroundsClass
{
  GInitiallyUnownedClass parent;
};

GType                   hd_available_backgrounds_get_type      (void);

HDAvailableBackgrounds *hd_available_backgrounds_new           (void);

GtkTreeModel           *hd_available_backgrounds_get_model     (HDAvailableBackgrounds *background);

void                    hd_available_backgrounds_add_with_file (HDAvailableBackgrounds *backgrounds,
                                                                HDBackground           *background,
                                                                const char             *label,
                                                                GFile                  *image_file);
void                    hd_available_backgrounds_add_with_icon (HDAvailableBackgrounds *backgrounds,
                                                                HDBackground           *background,
                                                                const char             *label,
                                                                GdkPixbuf              *icon);

void                    hd_available_backgrounds_run (HDAvailableBackgrounds *backgrounds,
                                                      guint                   current_view);
void                    hd_available_backgrounds_set_user_selected (HDAvailableBackgrounds *backgrounds,
                                                                    GFile                  *user_selected_file);


G_END_DECLS

#endif

