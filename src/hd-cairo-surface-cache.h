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

#ifndef __HD_CAIRO_SURFACE_CACHE_H__
#define __HD_CAIRO_SURFACE_CACHE_H__

#include <glib-object.h>
#include <cairo.h>

G_BEGIN_DECLS

#define HD_TYPE_CAIRO_SURFACE_CACHE             (hd_cairo_surface_cache_get_type ())
#define HD_CAIRO_SURFACE_CACHE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_CAIRO_SURFACE_CACHE, HDCairoSurfaceCache))
#define HD_CAIRO_SURFACE_CACHE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_CAIRO_SURFACE_CACHE, HDCairoSurfaceCacheClass))
#define HD_IS_CAIRO_SURFACE_CACHE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_CAIRO_SURFACE_CACHE))
#define HD_IS_CAIRO_SURFACE_CACHE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_CAIRO_SURFACE_CACHE))
#define HD_CAIRO_SURFACE_CACHE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_CAIRO_SURFACE_CACHE, HDCairoSurfaceCacheClass))

typedef struct _HDCairoSurfaceCache        HDCairoSurfaceCache;
typedef struct _HDCairoSurfaceCacheClass   HDCairoSurfaceCacheClass;
typedef struct _HDCairoSurfaceCachePrivate HDCairoSurfaceCachePrivate;

/** HDCairoSurfaceCache:
 *
 * A picker dialog for applets
 */
struct _HDCairoSurfaceCache
{
  GObject parent;

  HDCairoSurfaceCachePrivate *priv;
};

struct _HDCairoSurfaceCacheClass
{
  GObjectClass parent;
};

GType                hd_cairo_surface_cache_get_type    (void);

HDCairoSurfaceCache *hd_cairo_surface_cache_get         (void);
cairo_surface_t *    hd_cairo_surface_cache_get_surface (HDCairoSurfaceCache *cache,
                                                         const gchar         *filename);

G_END_DECLS

#endif

