/*
 * This file is part of hildon-desktop
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-multi-map.h"

#define HD_MULTI_MAP_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_MULTI_MAP, HDMultiMapPrivate))

struct _HDMultiMapPrivate
{
  GHashTable *map;
};

static void hd_multi_map_dispose     (GObject *object);

static GList *remove_value_from_list (GList   *list,
                                      GObject *value);

static void remove_and_free_all_keys_and_values (GHashTable *map);
static void free_values_list (GList *list);

G_DEFINE_TYPE (HDMultiMap, hd_multi_map, G_TYPE_INITIALLY_UNOWNED);

HDMultiMap *
hd_multi_map_new (void)
{
  HDMultiMap *multi_map;

  multi_map = g_object_new (HD_TYPE_MULTI_MAP,
                            NULL);

  return multi_map;
}

static void
hd_multi_map_class_init (HDMultiMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_multi_map_dispose;

  g_type_class_add_private (klass, sizeof (HDMultiMapPrivate));
}

static void
hd_multi_map_init (HDMultiMap *multi_map)
{
  HDMultiMapPrivate *priv;

  priv = multi_map->priv = HD_MULTI_MAP_GET_PRIVATE (multi_map);

  priv->map = g_hash_table_new_full (g_direct_hash,
                                     g_direct_equal,
                                     (GDestroyNotify) g_object_unref,
                                     NULL);
}

static void
hd_multi_map_dispose (GObject *object)
{
  HDMultiMap *multi_map = HD_MULTI_MAP (object);
  HDMultiMapPrivate *priv = multi_map->priv;

  if (priv->map)
    {
      hd_multi_map_remove_all (multi_map);
      priv->map = (g_hash_table_destroy (priv->map), NULL);
    }

  G_OBJECT_CLASS (hd_multi_map_parent_class)->dispose (object);
}

void
hd_multi_map_insert (HDMultiMap *multi_map,
                     GObject    *key,
                     GObject    *value)
{
  HDMultiMapPrivate *priv = multi_map->priv;
  GList *values;

  g_return_if_fail (HD_IS_MULTI_MAP (multi_map));

  values = g_hash_table_lookup (priv->map,
                                key);
  values = g_list_append (values, g_object_ref (value));
  g_hash_table_insert (priv->map,
                       g_object_ref (key),
                       values);
}

void
hd_multi_map_remove (HDMultiMap *multi_map,
                     GObject    *key,
                     GObject    *value)
{
  HDMultiMapPrivate *priv = multi_map->priv;
  GList *values;

  g_return_if_fail (HD_IS_MULTI_MAP (multi_map));

  values = g_hash_table_lookup (priv->map,
                                key);
  values = remove_value_from_list (values,
                                   value);
  if (values)
    g_hash_table_insert (priv->map,
                         g_object_ref (key),
                         values);
  else
    g_hash_table_remove (priv->map,
                         key);
}

static GList *
remove_value_from_list (GList   *list,
                        GObject *value)
{
  GList *link;

  link = g_list_find (list, value);
  if (link)
    {
      g_object_unref (link->data);
      list = g_list_delete_link (list, link);
    }

  return list;
}

void
hd_multi_map_remove_all (HDMultiMap *multi_map)
{
  HDMultiMapPrivate *priv = multi_map->priv;

  g_return_if_fail (HD_IS_MULTI_MAP (multi_map));

  remove_and_free_all_keys_and_values (priv->map);
}

static void
remove_and_free_all_keys_and_values (GHashTable *map)
{
  GHashTableIter iter;
  gpointer key, values;

  g_hash_table_iter_init (&iter, map);
  while (g_hash_table_iter_next (&iter, &key, &values)) 
    {
      free_values_list (values);
      g_hash_table_iter_remove (&iter);
    }
}

static void
free_values_list (GList *list)
{
  g_list_foreach (list, (GFunc) g_object_unref, NULL);
  g_list_free (list);
}

