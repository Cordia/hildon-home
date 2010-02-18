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

#include <glib/gi18n.h>

#include "hd-backgrounds.h"
#include "hd-file-background.h"
#include "hd-imageset-background.h"
#include "hd-theme-background.h"
#include "hd-wallpaper-background.h"

#include "hd-available-backgrounds.h"

#define HD_AVAILABLE_BACKGROUNDS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_AVAILABLE_BACKGROUNDS, HDAvailableBackgroundsPrivate))

struct _HDAvailableBackgroundsPrivate
{
  GtkListStore *backgrounds_store;
  GtkTreeModel *sorted_model;
  GtkTreeModel *filter_model;

  guint current_view;
  GFile *current_background;

  GFile *user_background;
  GtkTreePath *user_path;
};

static void hd_available_backgrounds_dispose (GObject *object);

enum
{
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (HDAvailableBackgrounds, hd_available_backgrounds, G_TYPE_INITIALLY_UNOWNED);

HDAvailableBackgrounds *
hd_available_backgrounds_new (void)
{
  HDAvailableBackgrounds *available_backgrounds;

  available_backgrounds = g_object_new (HD_TYPE_AVAILABLE_BACKGROUNDS,
                                        NULL);

  return available_backgrounds;
}

static void
hd_available_backgrounds_class_init (HDAvailableBackgroundsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_available_backgrounds_dispose;

  g_type_class_add_private (klass, sizeof (HDAvailableBackgroundsPrivate));

  signals [SELECTION_CHANGED] = g_signal_new ("selection-changed",
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_FIRST,
                                              0,
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__POINTER,
                                              G_TYPE_NONE, 1,
                                              G_TYPE_POINTER);
}

static gboolean
is_user_path_iter (HDAvailableBackgrounds *backgrounds,
                   GtkTreeIter            *sort_iter)
{
  HDAvailableBackgroundsPrivate *priv = backgrounds->priv;
  GtkTreePath *path;
  gboolean result = FALSE;

  if (!priv->user_path)
    return result;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->backgrounds_store),
                                  sort_iter);

  result = (gtk_tree_path_compare (path, priv->user_path) == 0);

  gtk_tree_path_free (path);

  return result;
}

static guint
get_priority (HDAvailableBackgrounds *backgrounds,
              GtkTreeModel           *model,
              GtkTreeIter            *iter)
{
  HDAvailableBackgroundsPrivate *priv = backgrounds->priv;
  guint priority = 0;
  gboolean is_ovi = FALSE;

  gtk_tree_model_get (model,
                      iter,
                      HD_BACKGROUND_COL_OVI, &is_ovi,
                      -1);
  if (is_ovi)
    return 4;

  if (!is_user_path_iter (backgrounds,
                          iter))
    {
      HDBackground *background;

      priority = 2;

      gtk_tree_model_get (model,
                          iter,
                          HD_BACKGROUND_COL_OBJECT, &background,
                          -1);

      if (background)
        {
          GFile *image_file;

          image_file = hd_background_get_image_file_for_view (background,
                                                              priv->current_view);


          if (g_file_equal (priv->current_background,
                            image_file))
            priority = 1;
          else if (HD_IS_THEME_BACKGROUND (background))
            priority = 3;

          g_object_unref (background);
        }
    }

  return priority;
}

static char *
get_name (GtkTreeModel *model,
          GtkTreeIter *iter)
{
  char *name;

  gtk_tree_model_get (model,
                      iter,
                      HD_BACKGROUND_COL_LABEL, &name,
                      -1);

  return name;
}

static gint
backgrounds_iter_cmp (GtkTreeModel           *model,
                      GtkTreeIter            *a,
                      GtkTreeIter            *b,
                      HDAvailableBackgrounds *backgrounds)
{
  guint priority_a, priority_b;

  priority_a = get_priority (backgrounds,
                             model,
                             a);
  priority_b = get_priority (backgrounds,
                             model,
                             b);

  if (priority_a != priority_b)
    {
      return priority_a - priority_b;
    }
  else
    {
      char *name_a, *name_b;
      gint result;

      name_a = get_name (model, a);
      name_b = get_name (model, b);

      if (name_a && name_b)
        result = g_utf8_collate (name_a, name_b);
      else
        result = name_a && !name_b ? 1 : -1;

      g_free (name_a);
      g_free (name_b);

      return result;
    }
}

static void
hd_available_backgrounds_init (HDAvailableBackgrounds *backgrounds)
{
  HDAvailableBackgroundsPrivate *priv;

  priv = backgrounds->priv = HD_AVAILABLE_BACKGROUNDS_GET_PRIVATE (backgrounds);

  priv->backgrounds_store = gtk_list_store_new (HD_BACKGROUND_NUM_COLS,
                                                G_TYPE_STRING,
                                                GDK_TYPE_PIXBUF,
                                                HD_TYPE_BACKGROUND,
                                                G_TYPE_BOOLEAN,
                                                G_TYPE_BOOLEAN);

  priv->sorted_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (priv->backgrounds_store));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (priv->sorted_model),
                                           (GtkTreeIterCompareFunc) backgrounds_iter_cmp,
                                           backgrounds,
                                           NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->sorted_model),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);

  priv->filter_model = gtk_tree_model_filter_new (priv->sorted_model,
                                                  NULL);
  gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                            HD_BACKGROUND_COL_VISIBLE);
}

static void
hd_available_backgrounds_dispose (GObject *object)
{
  HDAvailableBackgrounds *available_backgrounds = HD_AVAILABLE_BACKGROUNDS (object);
  HDAvailableBackgroundsPrivate *priv = available_backgrounds->priv;

  if (priv->user_path)
    priv->user_path = (gtk_tree_path_free (priv->user_path), NULL);

  if (priv->backgrounds_store)
    priv->backgrounds_store = (g_object_unref (priv->backgrounds_store), NULL);

  if (priv->sorted_model)
    priv->sorted_model = (g_object_unref (priv->sorted_model), NULL);

  if (priv->filter_model)
    priv->filter_model = (g_object_unref (priv->filter_model), NULL);

  G_OBJECT_CLASS (hd_available_backgrounds_parent_class)->dispose (object);
}

GtkTreeModel *
hd_available_backgrounds_get_model (HDAvailableBackgrounds *backgrounds)
{
  HDAvailableBackgroundsPrivate *priv;
  
  g_return_val_if_fail (HD_IS_AVAILABLE_BACKGROUNDS (backgrounds), NULL);

  priv = backgrounds->priv;

  return priv->filter_model;
}

static void
check_current_background (HDAvailableBackgrounds *backgrounds,
                          HDBackground           *background)
{
  HDAvailableBackgroundsPrivate *priv = backgrounds->priv;
  GFile *image_file;

  if (priv->user_background ||
      !priv->user_path)
    return;

  image_file = hd_background_get_image_file_for_view (background,
                                                      priv->current_view);

  if (g_file_equal (image_file,
                    priv->current_background))
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->backgrounds_store),
                               &iter,
                               priv->user_path);
      gtk_list_store_set (priv->backgrounds_store,
                          &iter,
                          HD_BACKGROUND_COL_VISIBLE, FALSE,
                          -1);
    } 
}

void
hd_available_backgrounds_add_with_file (HDAvailableBackgrounds *backgrounds,
                                        HDBackground           *background,
                                        const char             *label,
                                        GFile                  *image_file)
{
  HDAvailableBackgroundsPrivate *priv;
  GtkTreeIter iter;
  
  g_return_if_fail (HD_IS_AVAILABLE_BACKGROUNDS (backgrounds));

  priv = backgrounds->priv;

  gtk_list_store_insert_with_values (priv->backgrounds_store,
                                     &iter,
                                     -1,
                                     HD_BACKGROUND_COL_LABEL, label,
                                     HD_BACKGROUND_COL_OBJECT, background,
                                     HD_BACKGROUND_COL_VISIBLE, TRUE,
                                     HD_BACKGROUND_COL_OVI, FALSE,
                                     -1);

  hd_background_set_thumbnail_from_file (HD_BACKGROUND (background),
                                         GTK_TREE_MODEL (priv->backgrounds_store),
                                         &iter,
                                         image_file);

  check_current_background (backgrounds,
                            background);
}

void
hd_available_backgrounds_add_with_icon (HDAvailableBackgrounds *backgrounds,
                                        HDBackground           *background,
                                        const char             *label,
                                        GdkPixbuf              *icon)
{
  HDAvailableBackgroundsPrivate *priv;
  GtkTreeIter iter;
  
  g_return_if_fail (HD_IS_AVAILABLE_BACKGROUNDS (backgrounds));

  priv = backgrounds->priv;

  gtk_list_store_insert_with_values (priv->backgrounds_store,
                                     &iter,
                                     -1,
                                     HD_BACKGROUND_COL_LABEL, label,
                                     HD_BACKGROUND_COL_THUMBNAIL, icon,
                                     HD_BACKGROUND_COL_OBJECT, background,
                                     HD_BACKGROUND_COL_VISIBLE, TRUE,
                                     HD_BACKGROUND_COL_OVI, FALSE,
                                     -1);

  check_current_background (backgrounds,
                            background);
}

void
hd_available_backgrounds_run (HDAvailableBackgrounds *backgrounds,
                              guint                   current_view)
{
  HDAvailableBackgroundsPrivate *priv;
  HDBackground *background;
  char *label;
  GFile *image_file;
  GtkIconTheme *icon_theme;
  GdkPixbuf *ovi_icon = NULL;
  GtkTreeIter iter;

  g_return_if_fail (HD_IS_AVAILABLE_BACKGROUNDS (backgrounds));

  priv = backgrounds->priv;

  priv->current_view = current_view;
  priv->current_background = hd_backgrounds_get_background (hd_backgrounds_get (),
                                                            priv->current_view);

  g_assert (!priv->user_path);

  background = hd_file_background_new (priv->current_background);

  label = hd_file_background_get_label (HD_FILE_BACKGROUND (background));
  gtk_list_store_insert_with_values (priv->backgrounds_store,
                                     &iter,
                                     -1,
                                     HD_BACKGROUND_COL_LABEL, label,
                                     HD_BACKGROUND_COL_OBJECT, background,
                                     HD_BACKGROUND_COL_VISIBLE, TRUE,
                                     HD_BACKGROUND_COL_OVI, FALSE,
                                     -1);

  image_file = hd_file_background_get_image_file (HD_FILE_BACKGROUND (background));
  hd_background_set_thumbnail_from_file (background,
                                         GTK_TREE_MODEL (priv->backgrounds_store),
                                         &iter,
                                         image_file);
  g_object_unref (background);
  priv->user_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->backgrounds_store),
                                             &iter);

  hd_imageset_background_get_available (backgrounds);
  hd_wallpaper_background_get_available (backgrounds);
  hd_theme_background_get_available (backgrounds);

  /* Add the Ovi link at the end. */
  icon_theme = gtk_icon_theme_get_default ();
  if (icon_theme)
    ovi_icon = gtk_icon_theme_load_icon (icon_theme,
                                         "general_ovi_store",
                                         60, GTK_ICON_LOOKUP_NO_SVG,
                                         NULL);

  gtk_list_store_insert_with_values (priv->backgrounds_store,
                                     NULL,
                                     10000,
                                     HD_BACKGROUND_COL_THUMBNAIL, ovi_icon,
                                     HD_BACKGROUND_COL_LABEL, gettext ("home_li_get_ovi"),
                                     HD_BACKGROUND_COL_OBJECT, NULL,
                                     HD_BACKGROUND_COL_VISIBLE, TRUE,
                                     HD_BACKGROUND_COL_OVI, TRUE,
                                     -1);
}

void
hd_available_backgrounds_set_user_selected (HDAvailableBackgrounds *backgrounds,
                                            GFile                  *user_selected_file)
{
  HDAvailableBackgroundsPrivate *priv;
  HDBackground *background;
  char *label;
  GFile *image_file;
  GtkTreeIter iter, sort_iter, filter_iter;
  g_return_if_fail (HD_IS_AVAILABLE_BACKGROUNDS (backgrounds));

  priv = backgrounds->priv;

  priv->user_background = user_selected_file;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->backgrounds_store),
                           &iter,
                           priv->user_path);

  background = hd_file_background_new (priv->user_background);

  label = hd_file_background_get_label (HD_FILE_BACKGROUND (background));
  gtk_list_store_set (priv->backgrounds_store,
                      &iter,
                      HD_BACKGROUND_COL_LABEL, label,
                      HD_BACKGROUND_COL_OBJECT, background,
                      HD_BACKGROUND_COL_VISIBLE, TRUE,
                      -1);

  image_file = hd_file_background_get_image_file (HD_FILE_BACKGROUND (background));
  hd_background_set_thumbnail_from_file (background,
                                         GTK_TREE_MODEL (priv->backgrounds_store),
                                         &iter,
                                         image_file);
  g_object_unref (background);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (priv->sorted_model),
                                                  &sort_iter,
                                                  &iter);
  gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                                    &filter_iter,
                                                    &sort_iter);

  g_signal_emit (backgrounds,
                 signals [SELECTION_CHANGED],
                 0,
                 &filter_iter);
}

