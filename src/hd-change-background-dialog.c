/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008, 2009, 2010 Nokia Corporation.
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

#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-selection.h>

#include "hd-available-backgrounds.h"
#include "hd-backgrounds.h"

#include "hd-change-background-dialog.h"

/* Add Image dialog */
#define RESPONSE_ADD 1

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"

#define HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialogPrivate))

enum
{
  PROP_0,
  PROP_CURRENT_VIEW
};

struct _HDChangeBackgroundDialogPrivate
{
  HDAvailableBackgrounds *backgrounds;

  GtkWidget    *selector;

  GtkTreePath  *custom_image;

  guint  current_view;
  GFile *current_background;

  guint scroll_to_selected_id;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (HDChangeBackgroundDialog, hd_change_background_dialog, GTK_TYPE_DIALOG);

static gboolean
scroll_to_selected (gpointer data)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (data)->priv;
  GtkTreeIter iter;

  priv->scroll_to_selected_id = 0;

  if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                          0,
                                          &iter))
    {
      hildon_touch_selector_select_iter (HILDON_TOUCH_SELECTOR (priv->selector),
                                         0,
                                         &iter,
                                         TRUE);
    }

  return FALSE;
}

static void
backgrounds_selection_changed (HDAvailableBackgrounds   *backgrounds,
                               GtkTreeIter              *iter,
                               HDChangeBackgroundDialog *dialog)
{
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;

  hildon_touch_selector_select_iter (HILDON_TOUCH_SELECTOR (priv->selector),
                                     0,
                                     iter,
                                     FALSE);

  priv->scroll_to_selected_id = gdk_threads_add_idle (scroll_to_selected,
                                                      dialog);
}

static void
hd_change_background_dialog_constructed (GObject *object)
{
  HDChangeBackgroundDialog *dialog = HD_CHANGE_BACKGROUND_DIALOG (object);
  HDChangeBackgroundDialogPrivate *priv = dialog->priv;

  if (G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->constructed)
    G_OBJECT_CLASS (hd_change_background_dialog_parent_class)->constructed (object);

  hd_available_backgrounds_run (priv->backgrounds,
                                priv->current_view);

  g_signal_connect (priv->backgrounds, "selection-changed",
                    G_CALLBACK (backgrounds_selection_changed),
                    object);
}

static void
hd_change_background_dialog_dispose (GObject *object)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (object)->priv;

  if (priv->backgrounds)
    priv->backgrounds = (g_object_unref (priv->backgrounds), NULL);

  if (priv->scroll_to_selected_id)
    priv->scroll_to_selected_id = (g_source_remove (priv->scroll_to_selected_id), 0);

  if (priv->cancellable)
    priv->cancellable = (g_object_unref (priv->cancellable), NULL);

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

static void
add_image_dialog_response (GtkDialog                *dialog,
                           gint                      response_id,
                           HDChangeBackgroundDialog *cb_dialog)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG (cb_dialog)->priv;

  if (response_id == GTK_RESPONSE_OK)
    {
      /* An image was selected */
      gchar *uri;

      uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

      if (uri)
        {
          GFile *image_file;

          image_file = g_file_new_for_uri (uri);

          hd_available_backgrounds_set_user_selected (priv->backgrounds,
                                                      image_file);

          g_free (uri);
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
          GtkTreeModel *model;
          HDBackground *background;

          /* Get selected background image */
          model = hd_available_backgrounds_get_model (priv->backgrounds);
          gtk_tree_model_get (model, &iter,
                              HD_BACKGROUND_COL_OBJECT, &background,
                              -1);

          hildon_gtk_window_set_progress_indicator (GTK_WINDOW (dialog),
                                                    TRUE);
          gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);

          hd_background_set_for_current_view (background,
                                              priv->current_view,
                                              priv->cancellable);

          hd_backgrounds_add_done_cb (hd_backgrounds_get (),
                                      background_set_cb,
                                      dialog,
                                      NULL);
        }
      else
        {
          gtk_widget_destroy (GTK_WIDGET (dialog));
        }
    }
  else
    {
      if (priv->cancellable)
        g_cancellable_cancel (priv->cancellable);

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

static void
hd_change_background_dialog_init (HDChangeBackgroundDialog *dialog)
{
  HDChangeBackgroundDialogPrivate *priv = HD_CHANGE_BACKGROUND_DIALOG_GET_PRIVATE (dialog);
  HildonTouchSelectorColumn *column;
  GtkCellRenderer *renderer;

  dialog->priv = priv;

  priv->backgrounds = hd_available_backgrounds_new ();
  priv->cancellable = g_cancellable_new ();

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext ("maemo-af-desktop", "home_ti_change_backgr"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_add"), RESPONSE_ADD);
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  /* Create the touch selector */
  priv->selector = hildon_touch_selector_new ();

  /* Create empty column */
  column = hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                                hd_available_backgrounds_get_model (priv->backgrounds),
                                                NULL);

  /* Add the thumbnail renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "pixbuf", HD_BACKGROUND_COL_THUMBNAIL);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "xpad", HILDON_MARGIN_DOUBLE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "text", HD_BACKGROUND_COL_LABEL);

  g_object_set (column,
                "text-column", HD_BACKGROUND_COL_LABEL,
                NULL);

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

