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

#ifndef __HD_MULTI_MAP_H__
#define __HD_MULTI_MAP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_MULTI_MAP             (hd_multi_map_get_type ())
#define HD_MULTI_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_MULTI_MAP, HDMultiMap))
#define HD_MULTI_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_MULTI_MAP, HDMultiMapClass))
#define HD_IS_MULTI_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_MULTI_MAP))
#define HD_IS_MULTI_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_MULTI_MAP))
#define HD_MULTI_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_MULTI_MAP, HDMultiMapClass))

typedef struct _HDMultiMap        HDMultiMap;
typedef struct _HDMultiMapClass   HDMultiMapClass;
typedef struct _HDMultiMapPrivate HDMultiMapPrivate;

struct _HDMultiMap
{
  GInitiallyUnowned parent;

  HDMultiMapPrivate *priv;
};

struct _HDMultiMapClass
{
  GInitiallyUnownedClass parent;
};

GType       hd_multi_map_get_type   (void);

HDMultiMap *hd_multi_map_new        (void);

void        hd_multi_map_insert     (HDMultiMap *multi_map,
                                     GObject    *key,
                                     GObject    *value);
void        hd_multi_map_remove     (HDMultiMap *multi_map,
                                     GObject    *key,
                                     GObject    *value);
void        hd_multi_map_remove_all (HDMultiMap *multi_map);

G_END_DECLS

#endif

