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

#include <string.h>

#include <glib/gi18n.h>

#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-selection.h>

#include <hildon-thumbnail-factory.h>

#include "hd-backgrounds.h"

#include "hd-change-background-dialog.h"

/* Add Image dialog */
#define RESPONSE_ADD 1

/* background Key file values */
#define KEY_FILE_BACKGROUND_VALUE_TYPE "Background Image"
#define KEY_FILE_BACKGROUND_KEY_FILE "File"
#define KEY_FILE_BACKGROUND_KEY_ORDER "X-Order"
#define KEY_FILE_BACKGROUND_KEY_FILE_1 "X-File1"
#define KEY_FILE_BACKGROUND_KEY_FILE_2 "X-File2"
#define KEY_FILE_BACKGROUND_KEY_FILE_3 "X-File3"
#define KEY_FILE_BACKGROUND_KEY_FILE_4 "X-File4"

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"

/* Directories */
#define DIR_THEMES "/usr/share/themes"
#define DIR_BACKGROUNDS "/usr/share/backgrounds"

#define THEME_DESCRIPTION_FILE_NAME "index.theme"

enum
{
  COL_NAME,
  COL_ORDER,
  COL_IMAGE,
  COL_IMAGE_1,
  COL_IMAGE_2,
  COL_IMAGE_3,
  COL_IMAGE_4,
  COL_IMAGE_SET,
  COL_THEME,
  COL_THUMBNAIL,
  NUM_COLS
};

#define HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialogPrivate))

enum
{
  PROP_0,
  PROP_CURRENT_VIEW
};

struct _HDChangeBackgroundDialogPrivate
{
  GtkTreeModel *model;

  GtkWidget    *selector;

  GtkTreePath  *custom_image;

  guint         current_view;

  HildonThumbnailFactory *thumbnail_factory;
};

G_DEFINE_TYPE (HDChangeBackgroundDialog, hd_change_background_dialog, GTK_TYPE_DIALOG);

static gchar *
get_theme_description_path (const gchar *theme)
{
  return g_build_filename (DIR_THEMES,
                           theme,
                           THEME_DESCRIPTION_FILE_NAME,
                           NULL);
}

static gchar *
get_thumbnail_path_for_theme (const gchar *theme)
{
  gchar *theme_description_path = NULL;
  GKeyFile *theme_description;
  gchar *result = NULL;
  GError *error = NULL;

  theme_description_path = get_theme_description_path (theme);

  theme_description = g_key_file_new ();
  g_key_file_load_from_file (theme_description,
                             theme_description_path,
                             G_KEY_FILE_NONE,
                             &error);

  if (error)
    {
      g_warning ("%s. Could not read theme description file %s. %s",
                 __FUNCTION__,
                 theme_description_path,
                 error->message);
      g_error_free (error);
      goto clean;
    }

  result = g_key_file_get_string (theme_description,
                                  G_KEY_FILE_DESKTOP_GROUP,
                                  G_KEY_FILE_DESKTOP_KEY_ICON,
                                  &error);

  if (error)
    {
      g_warning ("%s. Could not read %s/%s from theme description file %s. %s",
                 __FUNCTION__,
                 G_KEY_FILE_DESKTOP_GROUP,
                 G_KEY_FILE_DESKTOP_KEY_ICON,
                 theme_description_path,
                 error->message);
      g_error_free (error);
      goto clean;
    }

clean:
  g_free (theme_description_path);
  g_key_file_free (theme_description);

  return result;
}

static GdkPixbuf *
get_thumbnail_for_theme (const gchar *theme)
{
  gchar *path;
  GdkPixbuf *thumbnail = NULL;
  GError *error = NULL;

  path = get_thumbnail_path_for_theme (theme);
  if (path)
    {
      thumbnail = gdk_pixbuf_new_from_file (path,
                                            &error);
      if (error)
        {
          g_warning ("%s. Could not read thumbnail icon %s. %s",
                     __FUNCTION__,
                     path,
                     error->message);
          g_error_free (error);
        }
    }

  return thumbnail;
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
                              COL_THUMBNAIL, thumbnail,
                              -1);
        }
    }
}

static void
hd_change_background_dialog_append_backgrounds (HDChangeBackgroundDialog  *dialog,
                                                const gchar               *dirname,
                                                const gchar               *theme,
                                                const gchar               *current_background,
                                                GtkTreeRowReference      **selected_row)
{
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;
  GDir *dir;
  const gchar *basename;

  dir = g_dir_open (dirname, 0, NULL);

  if (!dir)
    return;

  for (basename = g_dir_read_name (dir); basename; basename = g_dir_read_name (dir))
    {
      gchar *type, *filename, *name, *label = NULL;
      gchar *image[5];
      gboolean image_set = FALSE;
      GKeyFile *keyfile;
      GtkTreeIter iter;

      if (!g_str_has_suffix (basename, ".desktop") ||
          !strcmp ("default.desktop", basename))
        continue;

      keyfile = g_key_file_new ();

      /* Lod from file */
      filename = g_build_filename (dirname,
                                   basename,
                                   NULL);
      g_key_file_load_from_file (keyfile,
                                 filename,
                                 G_KEY_FILE_NONE,
                                 NULL);

      type = g_key_file_get_string (keyfile,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_TYPE,
                                    NULL);
      if (!type || strcmp (type, KEY_FILE_BACKGROUND_VALUE_TYPE))
        {
          /* No background .desktop file */

          g_key_file_free (keyfile);
          g_free (type);

          g_free (filename);
          continue;
        }

      name = g_key_file_get_string (keyfile,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_NAME,
                                    NULL);
      image[0] = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE,
                                        NULL);
      image[1] = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE_1,
                                        NULL);
      image[2] = g_key_file_get_string (keyfile,
                                       G_KEY_FILE_DESKTOP_GROUP,
                                       KEY_FILE_BACKGROUND_KEY_FILE_2,
                                       NULL);
      image[3] = g_key_file_get_string (keyfile,
                                       G_KEY_FILE_DESKTOP_GROUP,
                                       KEY_FILE_BACKGROUND_KEY_FILE_3,
                                       NULL);
      image[4] = g_key_file_get_string (keyfile,
                                       G_KEY_FILE_DESKTOP_GROUP,
                                       KEY_FILE_BACKGROUND_KEY_FILE_4,
                                       NULL);

      if (image[1] || image[2] || image[3] || image[4])
        {
          image_set = TRUE;

          if (!image[1] || !image[2] || !image[3] || !image[4])
            {
              g_warning ("%s. Image Set %s must define four images.",
                         __FUNCTION__,
                         filename);
              goto cleanup;
            }
        }
      

      if (theme && !image_set)
        {
          g_warning ("%s. Theme background %s has to be an image set",
                     __FUNCTION__,
                     filename);
          goto cleanup;
        }

      if (theme)
        {
          label = g_strdup_printf ("%s - %s",
                                   dgettext ("maemo-af-desktop",
                                             "home_ia_theme_prefix"),
                                   name);
        }
      else if (image_set)
        {
          label = g_strdup_printf ("%s - %s",
                                   dgettext ("maemo-af-desktop",
                                             "home_ia_imageset_prefix"),
                                   name);
        }
      else
        {
          label = g_strdup (name);
        }

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         &iter,
                                         -1,
                                         COL_NAME, label,
                                         COL_ORDER, 0,
                                         COL_IMAGE, image[0],
                                         COL_IMAGE_1, image[1],
                                         COL_IMAGE_2, image[2],
                                         COL_IMAGE_3, image[3],
                                         COL_IMAGE_4, image[4],
                                         COL_IMAGE_SET, image_set,
                                         COL_THEME, theme ? TRUE: FALSE,
                                         -1);

      /* Update thumbnail */
      if (theme)
        {
          GdkPixbuf *thumbnail;

          thumbnail = get_thumbnail_for_theme (theme);

          if (thumbnail)
            {
              gtk_list_store_set (GTK_LIST_STORE (priv->model),
                                  &iter,
                                  COL_THUMBNAIL, thumbnail,
                                  -1);

              g_object_unref (thumbnail);
            }
        }
      else if (image_set)
        {
          GtkTreePath *path;
          GtkTreeRowReference *reference;
          gchar *uri = g_filename_to_uri (image[1], NULL, NULL);

          path = gtk_tree_model_get_path (priv->model, &iter);
          reference = gtk_tree_row_reference_new (priv->model, path);
          gtk_tree_path_free (path);

          hildon_thumbnail_factory_request_pixbuf (priv->thumbnail_factory,
                                                   uri,
                                                   80, 60,
                                                   FALSE,
                                                   NULL,
                                                   image_set_thumbnail_callback,
                                                   reference,
                                                   (GDestroyNotify) gtk_tree_row_reference_free);

          g_free (uri);
        }

      if ((!image_set && !g_strcmp0 (image[0], current_background)) ||
          (image_set && !g_strcmp0 (image[priv->current_view + 1], current_background)))
        {
          GtkTreePath *path;

          path = gtk_tree_model_get_path (priv->model, &iter);

          if (selected_row)
            *selected_row = gtk_tree_row_reference_new (priv->model,
                                                        path);

          gtk_list_store_set (GTK_LIST_STORE (priv->model),
                              &iter,
                              COL_ORDER, -1,
                              -1);

          gtk_tree_path_free (path);
        }

cleanup:
      g_free (label);
      g_free (filename);
      g_key_file_free (keyfile);
      g_free (type);
      g_free (name);
      g_free (image[0]);
      g_free (image[1]);
      g_free (image[2]);
      g_free (image[3]);
      g_free (image[4]);
    }
  g_dir_close (dir);
}

static void
hd_change_background_dialog_constructed (GObject *object)
{
  HDChangeBackgroundDialog *dialog = HD_CHANGE_BACKGROUND_DIALOG (object);
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;
  GDir *dir;
  const gchar *current_background;
  GtkTreeRowReference *selected_row = NULL;
  gchar *images_dir;

  if (G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->constructed)
    G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->constructed (object);

  current_background = hd_backgrounds_get_background (hd_backgrounds_get (),
                                                      priv->current_view);

  /* Append backgrounds from themes */
  dir = g_dir_open (DIR_THEMES, 0, NULL);
  if (dir)
    {
      const gchar *basename;

      for (basename = g_dir_read_name (dir); basename; basename = g_dir_read_name (dir))
        {
          if (g_strcmp0 (basename, "default") != 0)
            {
              gchar *themes_dir;

              themes_dir = g_build_filename (DIR_THEMES,
                                             basename,
                                             "backgrounds",
                                             NULL);

              hd_change_background_dialog_append_backgrounds (dialog,
                                                              themes_dir,
                                                              basename,
                                                              current_background,
                                                              &selected_row);

              g_free (themes_dir);
            }
        }
      g_dir_close (dir);
    }

  /* Append backgrounds from background dir */
  hd_change_background_dialog_append_backgrounds (dialog,
                                                  DIR_BACKGROUNDS,
                                                  NULL,
                                                  current_background,
                                                  &selected_row);

  /* Append backgrounds from /home/user/MyDocs/.images */
  images_dir = g_build_filename (g_get_home_dir (),
                                 "MyDocs/.images",
                                 NULL);
  hd_change_background_dialog_append_backgrounds (dialog,
                                                  images_dir,
                                                  NULL,
                                                  current_background,
                                                  &selected_row);
  g_free (images_dir);

  if (selected_row)
    {
      GtkTreeIter iter;
      GtkTreePath *path = gtk_tree_row_reference_get_path (selected_row);

      gtk_tree_model_get_iter (priv->model, &iter, path);

      gtk_tree_path_free (path);

      hildon_touch_selector_select_iter (HILDON_TOUCH_SELECTOR (priv->selector),
                                         0,
                                         &iter,
                                         TRUE);
    }
  else
    {
      GtkTreeIter iter;
      gchar *label;

      label = g_filename_display_basename (current_background);

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         &iter,
                                         0,
                                         COL_NAME, label,
                                         COL_IMAGE, current_background,
                                         COL_ORDER, -2, /* first entry */
                                         COL_THEME, FALSE,
                                         COL_IMAGE_SET, FALSE,
                                         -1);

      priv->custom_image = gtk_tree_model_get_path (priv->model, &iter);

      hildon_touch_selector_select_iter (HILDON_TOUCH_SELECTOR (priv->selector),
                                         0,
                                         &iter,
                                         TRUE);
      g_free (label);
    }
}

static void
hd_change_background_dialog_dispose (GObject *object)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->thumbnail_factory)
    priv->thumbnail_factory = (g_object_unref (priv->thumbnail_factory), NULL);

  G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->dispose (object);
}

static void
hd_change_background_dialog_finalize (GObject *object)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv;

  if (priv->custom_image)
    priv->custom_image = (gtk_tree_path_free (priv->custom_image), NULL);

  G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->finalize (object);
}

static void
hd_change_background_dialog_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_CURRENT_VIEW:
      priv->current_view = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gchar *
get_image_label_from_uri (const gchar *uri)
{
  gchar *tmp;
  gchar *imagename;

  tmp = g_filename_from_uri (uri, NULL, NULL);
  if (!tmp)
    tmp = g_strdup (uri);

  imagename = g_filename_display_basename (tmp);

  g_free (tmp);

  return imagename;
}

static void
add_image_dialog_response (GtkDialog                *dialog,
                           gint                      response_id,
                           HDChangeBackgroundDialog *cb_dialog)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (cb_dialog)->priv;

  if (response_id == GTK_RESPONSE_OK)
    {
      /* An image was selected */
      gchar *uri, *label;
      GtkTreeIter iter;

      uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

      if (!uri)
        {
          g_warning ("No image file selected.");
        }
      else
        {
          gchar *filename = g_filename_from_uri (uri, NULL, NULL);

          /* Replace the existing custom image or create a new row 
           * if there is no image yet*/                            
          if (priv->custom_image)
            {
              gtk_tree_model_get_iter (priv->model, &iter, priv->custom_image);
            }
          else
            {
              gtk_list_store_insert (GTK_LIST_STORE (priv->model), &iter, 0);
              priv->custom_image = gtk_tree_model_get_path (priv->model, &iter);
            }

          label = get_image_label_from_uri (uri);

          gtk_list_store_set (GTK_LIST_STORE (priv->model), &iter,
                              COL_NAME, label,
                              COL_IMAGE, filename,
                              COL_ORDER, -2, /* first entry */
                              -1);

          hildon_touch_selector_select_iter (HILDON_TOUCH_SELECTOR (priv->selector),
                                             0,
                                             &iter,
                                             TRUE);

          g_free (uri);
          g_free (filename);
          g_free (label);
        }

    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
background_set_cb (gpointer data)
{
  if (GTK_IS_WIDGET (data))
    gtk_widget_destroy (GTK_WIDGET (data));

  return FALSE;
}

static void
hd_change_background_dialog_response (GtkDialog *dialog,
                                      gint       response_id)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (dialog)->priv;

  if (response_id == RESPONSE_ADD)
    {
      GtkWidget *add_image;
      GtkFileFilter *filter;
      gchar *images_folder;

      /* Create the Add Image dialog */
      add_image = hildon_file_chooser_dialog_new_with_properties (GTK_WINDOW (dialog),
                                                                  "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                  "title", dgettext (GETTEXT_PACKAGE, "home_ti_add_image"),
                                                                  "empty-text", dgettext (GETTEXT_PACKAGE, "home_li_no_images"),
                                                                  "open-button-text", dgettext ("hildon-libs", "wdgt_bd_done"),
                                                                  "select-multiple", FALSE,
                                                                  "selection-mode", HILDON_FILE_SELECTION_MODE_THUMBNAILS,
                                                                  "modal", FALSE,
                                                                  NULL);

      /* Filter for shown mime-types: JPG, GIF, PNG, BMP, TIFF, sketch.png */
      filter = gtk_file_filter_new ();
      gtk_file_filter_add_mime_type (filter, "image/jpeg");
      gtk_file_filter_add_mime_type (filter, "image/gif");
      gtk_file_filter_add_mime_type (filter, "image/png");
      gtk_file_filter_add_mime_type (filter, "image/bmp");
      gtk_file_filter_add_mime_type (filter, "image/tiff");
      gtk_file_filter_add_mime_type (filter, "sketch/png");
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (add_image),
                                   filter);

      /* Current folder by default: Images folder */
      images_folder = g_build_filename (g_get_home_dir (),
                                        USER_IMAGES_FOLDER,
                                        NULL);
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (add_image),
                                           images_folder);
      g_free (images_folder);

      /* Connect signal handler */
      g_signal_connect (add_image, "response",
                        G_CALLBACK (add_image_dialog_response),
                        dialog);

      /* Show Add Image dialog */
      gtk_dialog_run (GTK_DIALOG (add_image));
    }
  else if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkTreeIter iter;

      if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                              0,
                                              &iter))
        {
          gchar *image[5];
          gboolean image_set;
          guint i;

          /* Get selected background image */
          gtk_tree_model_get (priv->model, &iter,
                              COL_IMAGE, &image[0],
                              COL_IMAGE_1, &image[1],
                              COL_IMAGE_2, &image[2],
                              COL_IMAGE_3, &image[3],
                              COL_IMAGE_4, &image[4],
                              COL_IMAGE_SET, &image_set,
                              -1);

          hildon_gtk_window_set_progress_indicator (GTK_WINDOW (dialog),
                                                    TRUE);

          if (image_set)
            {
              guint i;

              hd_backgrounds_set_background (hd_backgrounds_get (),
                                             priv->current_view,
                                             image[priv->current_view + 1],
                                             NULL,
                                             NULL);

              for (i = 0; i < 4; i++)
                {
                  if (i != priv->current_view)
                    hd_backgrounds_set_background (hd_backgrounds_get (),
                                                   i,
                                                   image[i + 1],
                                                   i == 3 ||
                                                   (i == 2 && priv->current_view == 3) ?
                                                   background_set_cb : NULL,
                                                   dialog);
                }
            }
          else
            {
              hd_backgrounds_set_background (hd_backgrounds_get (),
                                             priv->current_view,
                                             image[0],
                                             background_set_cb,
                                             dialog);
            }

          /* free memory */
          for (i = 0; i < 5; i++)
            g_free (image[i]);
        }
      else
        {
          gtk_widget_destroy (GTK_WIDGET (dialog));
        }
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
hd_change_background_dialog_class_init (HDChangeBackgroundDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  object_class->constructed = hd_change_background_dialog_constructed;
  object_class->dispose = hd_change_background_dialog_dispose;
  object_class->finalize = hd_change_background_dialog_finalize;

  object_class->set_property = hd_change_background_dialog_set_property;

  dialog_class->response = hd_change_background_dialog_response;

  g_object_class_install_property (object_class,
                                   PROP_CURRENT_VIEW,
                                   g_param_spec_uint ("current-view",
                                                      "Current view",
                                                      "ID of the currently shown view",
                                                      0,
                                                      3,
                                                      0,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDChangeBackgroundDialogPrivate));
}

static gint
backgrounds_iter_cmp (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer      user_data)
{
  gint a_order, b_order;
  gboolean a_theme, b_theme, a_image_set, b_image_set;
  gchar *a_name, *b_name;
  gint result = -1;

  gtk_tree_model_get (model,
                      a,
                      COL_ORDER, &a_order,
                      COL_THEME, &a_theme,
                      COL_IMAGE_SET, &a_image_set,
                      COL_NAME, &a_name,
                      -1);

  gtk_tree_model_get (model,
                      b,
                      COL_ORDER, &b_order,
                      COL_THEME, &b_theme,
                      COL_IMAGE_SET, &b_image_set,
                      COL_NAME, &b_name,
                      -1);

  if (a_order != b_order)
    {
      result = a_order - b_order;
    }
  else  if (a_theme)
    {
      if (b_theme)
        result = strcmp (a_name, b_name);
      else if (b_image_set)
        result = 1;
      else
        result = -1;
    }
  else if (a_image_set)
    {
      if (b_theme)
        result = -1;
      else if (b_image_set)
        result = strcmp (a_name, b_name);
      else
        result = -1;
    }
  else
    {
      if (b_image_set)
        result = 1;
      else
        result = strcmp (a_name, b_name);
     }

  g_free (a_name);
  g_free (b_name);

  return result;
}

static void
hd_change_background_dialog_init (HDChangeBackgroundDialog *dialog)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE (dialog);
  HildonTouchSelectorColumn *column;
  GtkCellRenderer *renderer;

  dialog->priv = priv;

  priv->thumbnail_factory = hildon_thumbnail_factory_get_instance ();

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext ("maemo-af-desktop", "home_ti_change_backgr"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_add"), RESPONSE_ADD);
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  /* Create the touch selector */
  priv->selector = hildon_touch_selector_new ();

  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_INT,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                                     G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF);

  /* Sort by order */
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (priv->model),
                                           backgrounds_iter_cmp,
                                           NULL,
                                           NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);

  /* Create empty column */
  column = hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                                priv->model,
                                                NULL);

  /* Add the thumbnail renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "pixbuf", COL_THUMBNAIL);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "text", COL_NAME);

  gtk_widget_show (priv->selector);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), priv->selector);

  gtk_widget_set_size_request (GTK_WIDGET (dialog), -1, 358);
}

GtkWidget *
hd_change_background_dialog_new (guint current_view)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_CHANGE_BACKGROUND_DIALOG,
                         "current-view", current_view,
                         NULL);

  return window;
}

