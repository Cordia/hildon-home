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

#include "hd-object-vector.h"

#define HD_OBJECT_VECTOR_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_OBJECT_VECTOR, HDObjectVectorPrivate))

struct _HDObjectVectorPrivate
{
  GPtrArray *array;
};

static void hd_object_vector_dispose (GObject *object);

static void remove_all_objects_from_array (GPtrArray *array);

G_DEFINE_TYPE (HDObjectVector, hd_object_vector, G_TYPE_INITIALLY_UNOWNED);

HDObjectVector *
hd_object_vector_new (void)
{
  HDObjectVector *object_vector;

  object_vector = g_object_new (HD_TYPE_OBJECT_VECTOR,
                                NULL);

  return object_vector;
}

HDObjectVector *
hd_object_vector_new_at_size (size_t   n,
                              gpointer value)
{
  HDObjectVector *object_vector = hd_object_vector_new ();
  guint i;

  for (i = 0; i < n; i++)
    {
      hd_object_vector_push_back (object_vector,
                                  value);
    }

  return object_vector;
}

static void
hd_object_vector_class_init (HDObjectVectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_object_vector_dispose;

  g_type_class_add_private (klass, sizeof (HDObjectVectorPrivate));
}

static void
hd_object_vector_init (HDObjectVector *object_vector)
{
  HDObjectVectorPrivate *priv;

  priv = object_vector->priv = HD_OBJECT_VECTOR_GET_PRIVATE (object_vector);

  priv->array = g_ptr_array_new ();
}

static void
hd_object_vector_dispose (GObject *object)
{
  HDObjectVector *object_vector = HD_OBJECT_VECTOR (object);
  HDObjectVectorPrivate *priv = object_vector->priv;

  if (priv->array)
    {
      remove_all_objects_from_array (priv->array);
      g_ptr_array_free (priv->array, TRUE);
      priv->array = NULL;
    }

  G_OBJECT_CLASS (hd_object_vector_parent_class)->dispose (object);
}

gpointer
hd_object_vector_at (HDObjectVector *object_vector,
                     size_t          index)
{
  HDObjectVectorPrivate *priv;

  g_return_val_if_fail (HD_IS_OBJECT_VECTOR (object_vector), NULL);

  priv = object_vector->priv;

  g_return_val_if_fail (index < priv->array->len, NULL);

  return g_ptr_array_index (priv->array, index);
}

void
hd_object_vector_set_at (HDObjectVector *object_vector,
                         size_t          index,
                         gpointer        value)
{
  HDObjectVectorPrivate *priv;

  g_return_if_fail (HD_IS_OBJECT_VECTOR (object_vector));

  priv = object_vector->priv;

  g_return_if_fail (index < priv->array->len);

  if (value)
    g_object_ref_sink (value);

  if (g_ptr_array_index (priv->array, index))
    g_object_unref (g_ptr_array_index (priv->array, index));

  g_ptr_array_index (priv->array, index) = value;
}

void
hd_object_vector_clear (HDObjectVector *object_vector)
{
  HDObjectVectorPrivate *priv;

  g_return_if_fail (HD_IS_OBJECT_VECTOR (object_vector));

  priv = object_vector->priv;

  remove_all_objects_from_array (priv->array);
}

static void
remove_all_objects_from_array (GPtrArray *array)
{
  guint i;

  for (i = 0; i < array->len; i++)
    {
      gpointer value = g_ptr_array_index (array, i);
      if (value)
        g_object_unref (value);
    }

  g_ptr_array_remove_range (array, 0, array->len);
}

void
hd_object_vector_push_back (HDObjectVector *object_vector,
                            gpointer        value)
{
  HDObjectVectorPrivate *priv;

  g_return_if_fail (HD_IS_OBJECT_VECTOR (object_vector));
  g_return_if_fail (!value || G_IS_OBJECT (value));

  priv = object_vector->priv;

  if (value)
    g_object_ref_sink (value);

  g_ptr_array_add (priv->array,
                   value);
}

size_t
hd_object_vector_size (HDObjectVector *object_vector)
{
  HDObjectVectorPrivate *priv;

  g_return_val_if_fail (HD_IS_OBJECT_VECTOR (object_vector), 0);

  priv = object_vector->priv;

  return priv->array->len;
}

