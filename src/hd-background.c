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

#include <hildon-thumbnail-factory.h>

#include "hd-background.h"

#define HD_BACKGROUND_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_BACKGROUND, HDBackgroundPrivate))


struct _HDBackgroundPrivate
{
  HildonThumbnailFactory *thumbnail_factory;
};

G_DEFINE_ABSTRACT_TYPE (HDBackground, hd_background, G_TYPE_OBJECT);

static void
hd_background_dispose (GObject *object)
{
  HDBackgroundPrivate *priv = HD_BACKGROUND (object)->priv;

  if (priv->thumbnail_factory)
    priv->thumbnail_factory = (g_object_unref (priv->thumbnail_factory), NULL);

  G_OBJECT_CLASS (hd_background_parent_class)->dispose (object);
}

static void
hd_background_class_init (HDBackgroundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_background_dispose;

  g_type_class_add_private (klass, sizeof (HDBackgroundPrivate));
}

static void
hd_background_init (HDBackground *background)
{
  HDBackgroundPrivate *priv = HD_BACKGROUND_GET_PRIVATE (background);

  background->priv = priv;

  priv->thumbnail_factory = hildon_thumbnail_factory_get_instance ();
}

static void
image_set_thumbnail_callback (HildonThumbnailFactory *self,
                              GdkPixbuf              *thumbnail,
                              GError                 *error,
                              gpointer                user_data)
{
  GtkTreeRowReference *reference = user_data;
  GtkTreeModel *model = gtk_tree_row_reference_get_model (reference);
  GtkTreePath *path = gtk_tree_row_reference_get_path (reference);

  if (path && thumbnail)
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter (model, &iter, path))
        {
          gtk_list_store_set (GTK_LIST_STORE (model),
                              &iter,
                              HD_BACKGROUND_COL_THUMBNAIL, thumbnail,
                              -1);
        }

      gtk_tree_path_free (path);
    }
}

void
hd_background_set_thumbnail_from_file (HDBackground *background,
                                       GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       GFile        *file)
{
  HDBackgroundPrivate *priv = background->priv;
  GtkTreePath *path;
  GtkTreeRowReference *reference;
  char *uri;

  path = gtk_tree_model_get_path (model, iter);
  reference = gtk_tree_row_reference_new (model, path);

  uri = g_file_get_uri (file);

  hildon_thumbnail_factory_request_pixbuf (priv->thumbnail_factory,
                                           uri,
                                           80, 60,
                                           FALSE,
                                           NULL,
                                           image_set_thumbnail_callback,
                                           reference,
                                           (GDestroyNotify) gtk_tree_row_reference_free);

  gtk_tree_path_free (path);
  g_free (uri);
}

void
hd_background_set_for_current_view (HDBackground *background,
                                    guint         view,
                                    GCancellable *cancellable)
{
  HD_BACKGROUND_GET_CLASS (background)->set_for_current_view (background,
                                                              view,
                                                              cancellable);
}

GFile *
hd_background_get_image_file_for_view (HDBackground *background,
                                       guint         view)
{
  if (HD_BACKGROUND_GET_CLASS (background)->get_image_file_for_view)
   return HD_BACKGROUND_GET_CLASS (background)->get_image_file_for_view (background,
                                                                         view);

  return NULL;
}

/*
const gchar *
hd_background_get_title (HDBackground *background)
{
   g_return_val_if_fail (HD_IS_BACKGROUND (background), NULL);

   return NULL;
}

GdkPixbuf *
hd_background_get_thumbnail (HDBackground *background)
{
   g_return_val_if_fail (HD_IS_BACKGROUND (background), NULL);

   return NULL;
}
*/
