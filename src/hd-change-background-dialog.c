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

#include <gconf/gconf-client.h>

#include "hd-change-background-dialog.h"

/* Pixel sizes */
#define CHANGE_BACKGROUND_DIALOG_WIDTH 342
#define CHANGE_BACKGROUND_DIALOG_HEIGHT 80

#define CHANGE_BACKGROUND_DIALOG_CLOSE  43
#define CHANGE_BACKGROUND_DIALOG_ICON  24

#define MARGIN_DEFAULT 8
#define MARGIN_HALF 4

/* Timeout in seconds */
#define CHANGE_BACKGROUND_DIALOG_PREVIEW_TIMEOUT 4

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

/* Background gconf key */
#define GCONF_BACKGROUND_KEY(i) g_strdup_printf ("/apps/osso/hildon-desktop/views/%u/bg-image", i)

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"

/* Directories */
#define DIR_THEMES "/usr/share/themes"
#define DIR_BACKGROUNDS "/usr/share/backgrounds"

enum
{
  COL_NAME,
  COL_ORDER,
  COL_IMAGE,
  COL_IMAGE_1,
  COL_IMAGE_2,
  COL_IMAGE_3,
  COL_IMAGE_4,
  NUM_COLS
};

#define HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialogPrivate))

enum
{
  PROP_0,
  PROP_PROXY
};

struct _HDChangeBackgroundDialogPrivate
{
  GtkTreeModel *model;

  GtkWidget    *selector;

  GtkTreePath  *custom_image;

  DBusGProxy   *proxy;
};

G_DEFINE_TYPE (HDChangeBackgroundDialog, hd_change_background_dialog, GTK_TYPE_DIALOG);

static void
hd_change_background_dialog_dispose (GObject *object)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->proxy)
    priv->proxy = (g_object_unref (priv->proxy), NULL);

  G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->dispose (object);
}

static void
hd_change_background_dialog_finalize (GObject *object)
{
  /* HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv; */

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
    case PROP_PROXY:
      if (priv->proxy)
        g_object_unref (priv->proxy);
      priv->proxy = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_change_background_dialog_class_init (HDChangeBackgroundDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_change_background_dialog_dispose;
  object_class->finalize = hd_change_background_dialog_finalize;

  object_class->set_property = hd_change_background_dialog_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PROXY,
                                   g_param_spec_object ("proxy",
                                                        "Proxy",
                                                        "D-Bus proxy to the hildon-desktop Home object",
                                                        DBUS_TYPE_G_PROXY,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDChangeBackgroundDialogPrivate));
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
response_cb (HDChangeBackgroundDialog *dialog,
             gint             response_id,
             gpointer         data)
{
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;

  g_debug ("response_cb called %d", response_id);

  if (response_id == RESPONSE_ADD)
    {
      GtkWidget *add_image;
      GtkFileFilter *filter;
      gchar *images_folder;

      add_image = hildon_file_chooser_dialog_new_with_properties (GTK_WINDOW (dialog),
                                                                  "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                  "title", _("home_ti_add_image"),
                                                                  "empty-text", _("home_li_no_images"),
                                                                  "open-button-text", _("wdgt_bd_done"),
                                                                  "select-multiple", FALSE,
                                                                  "selection-mode", HILDON_FILE_SELECTION_MODE_THUMBNAILS,
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

      if (gtk_dialog_run (GTK_DIALOG (add_image)) == GTK_RESPONSE_OK)
        {
          /* An image was selected */
          gchar *uri, *label;
          GtkTreeIter iter;

          uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (add_image));

          if (!uri)
            {
              g_warning ("No image file selected.");
            }
          else
            {
              gchar *filename = g_filename_from_uri (uri, NULL, NULL);
                            
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
                                  COL_ORDER, -1, /* first entry */
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

      gtk_widget_destroy (GTK_WIDGET (add_image));
    }
  else if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkTreeIter iter;

      if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                              0,
                                              &iter))
        {
          gchar *image[5];
          GConfClient *client;
          guint i;

          gtk_tree_model_get (priv->model, &iter,
                              COL_IMAGE, &image[0],
                              COL_IMAGE_1, &image[1],
                              COL_IMAGE_2, &image[2],
                              COL_IMAGE_3, &image[3],
                              COL_IMAGE_4, &image[4],
                              -1);

          g_debug ("Selected: %s", image[0]);

          client = gconf_client_get_default ();

          if (image[1] || image[2] || image[3] || image[4])
            {
              /* Set backgrounds of all views */
              gchar *key;

              for (i = 1; i <= 4; i++)
                {
                  GError *error = NULL;

                  /* ignore not set images */
                  if (!image[i])
                    continue;

                  key = GCONF_BACKGROUND_KEY (i);
                  gconf_client_set_string (client,
                                           key,
                                           image[i],
                                           &error);

                  if (error)
                    {
                      g_warning ("Could not set background image for view %u, '%s'. %s",
                                 i, key, error->message);
                      g_error_free (error);
                    }

                  g_free (key);
                }
            }
          else
            {
              /* Set background of current view only */
              gchar *key;
              guint current_view;
              GError *error = NULL;

              dbus_g_proxy_call (priv->proxy,
                                 "GetCurrentViewID",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_UINT, &current_view,
                                 G_TYPE_INVALID);
              if (error)
                {
                  g_warning ("Could not get ID of current view. %s", error->message);

                  g_error_free (error);

                  current_view = 0;

                  error = NULL;
                }

              /* ID is from 0 to 3 */
              current_view++;

              g_debug ("current_view: %u", current_view);

              key = GCONF_BACKGROUND_KEY (current_view);
              gconf_client_set_string (client,
                                       key,
                                       image[0],
                                       &error);

              if (error)
                {
                  g_warning ("Could not set background image for view %u, '%s'. %s",
                             current_view, key, error->message);
                  g_error_free (error);
                }

              g_free (key);
            }

          /* free memory */
          for (i = 0; i < 5; i++)
            g_free (image[i]);
          g_object_unref (client);
        }

      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
hd_change_background_dialog_append_backgrounds (HDChangeBackgroundDialog *dialog,
                                                const gchar              *dirname)
{
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;
  GDir *dir;
  const gchar *basename;

  dir = g_dir_open (dirname, 0, NULL);

  if (!dir)
    return;

  for (basename = g_dir_read_name (dir); basename; basename = g_dir_read_name (dir))
    {
      gchar *type, *filename, *name;
      gchar *image, *image_1, *image_2, *image_3, *image_4;
      guint order;
      GKeyFile *keyfile;

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
      g_free (filename);

      type = g_key_file_get_string (keyfile,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_TYPE,
                                    NULL);
      if (!type || strcmp (type, KEY_FILE_BACKGROUND_VALUE_TYPE))
        {
          /* No background .desktop file */

          g_key_file_free (keyfile);
          g_free (type);

          continue;
        }

      name = g_key_file_get_string (keyfile,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_NAME,
                                    NULL);
      order = g_key_file_get_integer (keyfile,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      KEY_FILE_BACKGROUND_KEY_ORDER,
                                      NULL);
      image = g_key_file_get_string (keyfile,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      KEY_FILE_BACKGROUND_KEY_FILE,
                                      NULL);
      image_1 = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE_1,
                                        NULL);
      image_2 = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE_2,
                                        NULL);
      image_3 = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE_3,
                                        NULL);
      image_4 = g_key_file_get_string (keyfile,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        KEY_FILE_BACKGROUND_KEY_FILE_4,
                                        NULL);

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         NULL,
                                         -1,
                                         COL_NAME, name,
                                         COL_ORDER, order,
                                         COL_IMAGE, image,
                                         COL_IMAGE_1, image_1,
                                         COL_IMAGE_2, image_2,
                                         COL_IMAGE_3, image_3,
                                         COL_IMAGE_4, image_4,
                                         -1);

      g_key_file_free (keyfile);
      g_free (type);
      g_free (name);
    }
  g_dir_close (dir);
}
 
static void
hd_change_background_dialog_init (HDChangeBackgroundDialog *dialog)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE (dialog);
  HildonTouchSelectorColumn *column;
  GtkCellRenderer *renderer;
  GDir *dir;
  const gchar *basename;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("home_ti_change_backgr"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("wdgt_bd_add"), RESPONSE_ADD);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  /* */
  priv->selector = hildon_touch_selector_new ();

  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_INT,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  /* Append backgrounds from themes */
  dir = g_dir_open (DIR_THEMES, 0, NULL);
  if (dir)
    {
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
                                                              themes_dir);

              g_free (themes_dir);
            }
        }
      g_dir_close (dir);
    }

  /* Append backgrounds from background dir */
  hd_change_background_dialog_append_backgrounds (dialog,
                                                  DIR_BACKGROUNDS);
 
  /* Sort by order */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        COL_ORDER,
                                        GTK_SORT_ASCENDING);

  /* Create empty column */
  column = hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                                priv->model,
                                                NULL);
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


  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (response_cb), NULL);
}

GtkWidget *
hd_change_background_dialog_new (DBusGProxy *proxy)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_CHANGE_BACKGROUND_DIALOG,
                         "proxy", proxy,
                         NULL);

  return window;
}

