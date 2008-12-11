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

#include "hd-activate-views-dialog.h"

/* Pixel sizes */
#define ACTIVATE_VIEWS_DIALOG_WIDTH 342
#define ACTIVATE_VIEWS_DIALOG_HEIGHT 80

#define ACTIVATE_VIEWS_DIALOG_CLOSE  43
#define ACTIVATE_VIEWS_DIALOG_ICON  24

#define MARGIN_DEFAULT 8
#define MARGIN_HALF 4

/* Timeout in seconds */
#define ACTIVATE_VIEWS_DIALOG_PREVIEW_TIMEOUT 4

/* Add Image dialog */
#define RESPONSE_ADD 1

/* Background gconf key */
#define GCONF_BACKGROUND_KEY(i) g_strdup_printf ("/apps/osso/hildon-desktop/views/%u/bg-image", i)
#define GCONF_ACTIVE_KEY(i) g_strdup_printf ("/apps/osso/hildon-desktop/views/%u/active", i)

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"

enum
{
  COL_PIXBUF,
  NUM_COLS
};

#define HD_ACTIVATE_VIEWS_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_ACTIVATE_VIEWS_DIALOG, HDActivateViewsDialogPrivate))

struct _HDActivateViewsDialogPrivate
{
  GtkTreeModel *model;

  GtkWidget    *icon_view;
};

G_DEFINE_TYPE (HDActivateViewsDialog, hd_activate_views_dialog, GTK_TYPE_DIALOG);

static void
hd_activate_views_dialog_dispose (GObject *object)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (object)->priv;

  if (priv->model)
    {
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  G_OBJECT_CLASS (hd_activate_views_dialog_parent_class)->dispose (object);
}

static void
hd_activate_views_dialog_finalize (GObject *object)
{
  /* HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (object)->priv; */

  G_OBJECT_CLASS (hd_activate_views_dialog_parent_class)->finalize (object);
}

static void
hd_activate_views_dialog_class_init (HDActivateViewsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_activate_views_dialog_dispose;
  object_class->finalize = hd_activate_views_dialog_finalize;

  g_type_class_add_private (klass, sizeof (HDActivateViewsDialogPrivate));
}

static void
response_cb (HDActivateViewsDialog *dialog,
             gint             response_id,
             gpointer         data)
{
  HDActivateViewsDialogPrivate *priv = dialog->priv;

  g_debug ("response_cb called %d", response_id);

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (priv->model, &iter))
        {
          GConfClient *client = gconf_client_get_default ();
          guint i = 1;

          do
            {
              GtkTreePath *path;
              gchar *key;
              GError *error = NULL;

              path = gtk_tree_model_get_path (priv->model, &iter);

              key = GCONF_ACTIVE_KEY (i++);
              gconf_client_set_bool (client,
                                     key,
                                     gtk_icon_view_path_is_selected (GTK_ICON_VIEW (priv->icon_view),
                                                                     path),
                                     &error);

              if (error)
                {
                  g_warning ("Could not activate/deactivate view via GConf. %s", error->message);
                  g_error_free (error);
                }

              gtk_tree_path_free (path);
              g_free (key);
            }
          while (gtk_tree_model_iter_next (priv->model, &iter));

          g_object_unref (client);
        }
    }
}

static void
hd_activate_views_dialog_init (HDActivateViewsDialog *dialog)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG_GET_PRIVATE (dialog);
  GtkCellRenderer *renderer;
  guint i;
  GtkWidget *pannable;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("home_ti_manage_views"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  /* Create Touch grid list */
  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF);

  priv->icon_view = hildon_gtk_icon_view_new_with_model (HILDON_UI_MODE_EDIT,
                                                         priv->model);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (priv->icon_view),
                                    GTK_SELECTION_MULTIPLE);
  gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->icon_view),
                             4);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->icon_view),
                                  renderer,
                                  "pixbuf", COL_PIXBUF,
                                  NULL);

  /* Append views */
  for (i = 1; i <= 4; i++)
    {
      GError *error = NULL;
      gchar *key;
      gchar *bg_image;
      GdkPixbuf *pixbuf = NULL;
      GConfClient *client = gconf_client_get_default ();
      GtkTreeIter iter;
      GtkTreePath *path = NULL;

      key = GCONF_BACKGROUND_KEY (i);
      bg_image = gconf_client_get_string (client,
                                          key,
                                          &error);
      g_free (key);

      if (error)
        {
          g_warning ("Could not get background image for view %u, '%s'. %s",
                     i, key, error->message);
          g_error_free (error);
          error = NULL;
        }
      else
        pixbuf = gdk_pixbuf_new_from_file_at_scale (bg_image, 125, 75, TRUE, &error);

      if (error)
        {
          g_warning ("Could not get background image for view %u, '%s'. %s",
                     i, bg_image, error->message);
          g_error_free (error);
          error = NULL;

          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                   TRUE,
                                   8,
                                   125, 75);
          gdk_pixbuf_fill (pixbuf,
                           0x000000ff);
        }

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         &iter,
                                         -1,
                                         COL_PIXBUF, pixbuf,
                                         -1);

      path = gtk_tree_model_get_path (priv->model, &iter);

      key = GCONF_ACTIVE_KEY (i);
      if (gconf_client_get_bool (client,
                                 key,
                                 &error))
        {
          gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view),
                                     path);
        }
      else
        {
          gtk_icon_view_unselect_path (GTK_ICON_VIEW (priv->icon_view),
                                       path);
        }
      g_free (key);

      g_object_unref (client);
      if (pixbuf)
        g_object_unref (pixbuf);
      if (path)
        gtk_tree_path_free (path);
    }

  gtk_widget_show (priv->icon_view);

  pannable = hildon_pannable_area_new ();
  g_object_set (pannable,
                "mov_mode", 0,
                "vscrollbar_policy", GTK_POLICY_NEVER,
                "hscrollbar_policy", GTK_POLICY_NEVER,
/*                "size-request-policy", HILDON_SIZE_REQUEST_CHILDREN, */
                NULL);
  gtk_widget_show (pannable);
  hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable),
                                          priv->icon_view);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
/*                     priv->icon_view); */
                    pannable);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (response_cb), NULL);
}

GtkWidget *
hd_activate_views_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ACTIVATE_VIEWS_DIALOG,
                         NULL);

  return window;
}

