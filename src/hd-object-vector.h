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

#ifndef __HD_OBJECT_VECTOR_H__
#define __HD_OBJECT_VECTOR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_OBJECT_VECTOR             (hd_object_vector_get_type ())
#define HD_OBJECT_VECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_OBJECT_VECTOR, HDObjectVector))
#define HD_OBJECT_VECTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_OBJECT_VECTOR, HDObjectVectorClass))
#define HD_IS_OBJECT_VECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_OBJECT_VECTOR))
#define HD_IS_OBJECT_VECTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_OBJECT_VECTOR))
#define HD_OBJECT_VECTOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_OBJECT_VECTOR, HDObjectVectorClass))

typedef struct _HDObjectVector        HDObjectVector;
typedef struct _HDObjectVectorClass   HDObjectVectorClass;
typedef struct _HDObjectVectorPrivate HDObjectVectorPrivate;

struct _HDObjectVector
{
  GInitiallyUnowned parent;

  HDObjectVectorPrivate *priv;
};

struct _HDObjectVectorClass
{
  GInitiallyUnownedClass parent;
};

GType           hd_object_vector_get_type    (void);

HDObjectVector *hd_object_vector_new         (void);
HDObjectVector *hd_object_vector_new_at_size (size_t          n,
                                              gpointer        value);

gpointer        hd_object_vector_at          (HDObjectVector *object_vector,
                                              size_t          index);
void            hd_object_vector_set_at      (HDObjectVector *object_vector,
                                              size_t          index,
                                              gpointer        value);
void            hd_object_vector_clear       (HDObjectVector *object_vector);
void            hd_object_vector_push_back   (HDObjectVector *object_vector,
                                              gpointer        value);
size_t          hd_object_vector_size        (HDObjectVector *object_vector);

G_END_DECLS

#endif

