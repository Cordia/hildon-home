/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-cairo-surface-cache.h"

#define HD_CAIRO_SURFACE_CACHE_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_CAIRO_SURFACE_CACHE, HDCairoSurfaceCachePrivate))

struct _HDCairoSurfaceCachePrivate
{
  GHashTable *table;
};

G_DEFINE_TYPE (HDCairoSurfaceCache, hd_cairo_surface_cache, G_TYPE_OBJECT);

static void
hd_cairo_surface_cache_dispose (GObject *object)
{
  HDCairoSurfaceCachePrivate *priv = HD_CAIRO_SURFACE_CACHE (object)->priv;

  if (priv->table)
    priv->table = (g_hash_table_destroy (priv->table), NULL);

  G_OBJECT_CLASS (hd_cairo_surface_cache_parent_class)->dispose (object);
}

static void
hd_cairo_surface_cache_class_init (HDCairoSurfaceCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_cairo_surface_cache_dispose;

  g_type_class_add_private (klass, sizeof (HDCairoSurfaceCachePrivate));
}


static void
hd_cairo_surface_cache_init (HDCairoSurfaceCache *cache)
{
  HDCairoSurfaceCachePrivate *priv = HD_CAIRO_SURFACE_CACHE_GET_PRIVATE (cache);

  cache->priv = priv;

  priv->table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free,
                                       (GDestroyNotify) cairo_surface_destroy);
}

HDCairoSurfaceCache *
hd_cairo_surface_cache_get (void)
{
  static HDCairoSurfaceCache *cache = NULL;

  if (G_UNLIKELY (!cache))
    cache = g_object_new (HD_TYPE_CAIRO_SURFACE_CACHE,
                          NULL);

  return cache;
}

cairo_surface_t *
hd_cairo_surface_cache_get_surface (HDCairoSurfaceCache *cache,
                                    const gchar         *filename)
{
  HDCairoSurfaceCachePrivate *priv = cache->priv;
  cairo_surface_t *surface;

  surface = g_hash_table_lookup (priv->table,
                                 filename);

  if (!surface)
    {
      surface = cairo_image_surface_create_from_png (filename);
      g_hash_table_insert (priv->table,
                           g_strdup (filename),
                           surface);
    }

  return cairo_surface_reference (surface);
}
