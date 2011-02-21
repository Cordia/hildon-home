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

#include "hd-desktop.h"

#include "hd-activate-views-dialog.h"

#define HD_GCONF_KEY_ACTIVE_VIEWS "/apps/osso/hildon-desktop/views/active"

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"
#define HD_DESKTOP_VIEWS_COLUMNS 4
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

  GtkTreePath  *only_selected;

  GConfClient  *gconf_client;
};

G_DEFINE_TYPE (HDActivateViewsDialog, hd_activate_views_dialog, GTK_TYPE_DIALOG);

static void
hd_activate_views_dialog_dispose (GObject *object)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->gconf_client)
    priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);

  if (priv->only_selected)
    priv->only_selected = (gtk_tree_path_free (priv->only_selected), NULL);

  G_OBJECT_CLASS (hd_activate_views_dialog_parent_class)->dispose (object);
}

static void
hd_activate_views_dialog_response (GtkDialog *dialog,
                                   gint       response_id)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (dialog)->priv;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (priv->model, &iter))
        {
          GSList *list = NULL;
          gint i = 1;
          GError *error = NULL;

          do
            {
              GtkTreePath *path;

              path = gtk_tree_model_get_path (priv->model, &iter);

              if (gtk_icon_view_path_is_selected (GTK_ICON_VIEW (priv->icon_view),
                                                  path))
                list = g_slist_append (list, GINT_TO_POINTER (i));

              gtk_tree_path_free (path);

              i++;
            }
          while (gtk_tree_model_iter_next (priv->model, &iter));

          gconf_client_set_list (priv->gconf_client,
                                 HD_GCONF_KEY_ACTIVE_VIEWS,
                                 GCONF_VALUE_INT,
                                 list,
                                 &error);
          if (error)
            {
              g_warning ("Could not activate/deactivate view via GConf. %s", error->message);
              g_error_free (error);
            }

          g_slist_free (list);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
selection_changed_cb (GtkIconView           *icon_view,
                      HDActivateViewsDialog *dialog)
{
  HDActivateViewsDialogPrivate *priv = dialog->priv;
  GList *selected;

  /* Select at least one item and/or store the only selected item */
  selected = gtk_icon_view_get_selected_items (icon_view);

  g_debug ("%s, %p", __FUNCTION__, selected);
  if (selected)
    {
      if (priv->only_selected)
        priv->only_selected = (gtk_tree_path_free (priv->only_selected), NULL);

      if (selected->next)
        g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
      else
        priv->only_selected = selected->data;
    }
  else
    {
      gtk_icon_view_select_path (icon_view,
                                 priv->only_selected);
    }
  g_list_free (selected);
}

static void
hd_activate_views_dialog_class_init (HDActivateViewsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  object_class->dispose = hd_activate_views_dialog_dispose;

  dialog_class->response = hd_activate_views_dialog_response;

  g_type_class_add_private (klass, sizeof (HDActivateViewsDialogPrivate));
}

static void
hd_activate_views_dialog_init (HDActivateViewsDialog *dialog)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG_GET_PRIVATE (dialog);
  GtkCellRenderer *renderer;
  guint i;
  GtkWidget *pannable;
  GSList *list = NULL;
  gboolean active_views[HD_DESKTOP_VIEWS] = { 0,};
  gboolean none_active = TRUE;
  GList *selected;
  GError *error = NULL;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext (GETTEXT_PACKAGE, "home_ti_manage_views"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  /* Create Touch grid list */
  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF);

  priv->icon_view = hildon_gtk_icon_view_new_with_model (HILDON_UI_MODE_EDIT,
                                                         priv->model);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (priv->icon_view),
                                    GTK_SELECTION_MULTIPLE);
  gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->icon_view),
                             HD_DESKTOP_VIEWS_COLUMNS);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->icon_view),
                                  renderer,
                                  "pixbuf", COL_PIXBUF,
                                  NULL);

  /* Read active views from GConf */
  priv->gconf_client = gconf_client_get_default ();
  list = gconf_client_get_list (priv->gconf_client,
                                HD_GCONF_KEY_ACTIVE_VIEWS,
                                GCONF_VALUE_INT,
                                &error);

  if (!error)
    {

      GSList *l;

      for (l = list; l; l = l->next)
        {
          gint id = GPOINTER_TO_INT (l->data);

          /* Stored in GConf 1..HD_DESKTOP_VIEWS */

          if (id > 0 && id <= HD_DESKTOP_VIEWS)
            {
              active_views[id - 1] = TRUE;
              none_active = FALSE;
            }
        }
    }
  else
    {
      /* Error */
      g_warning ("Error reading active views from GConf. %s", error->message);
      error = (g_error_free (error), NULL);
    }

  g_slist_free (list);

  /* Check if there is an view active */
  if (none_active)
    {
      g_warning ("No active views. Make first view active");
      active_views[0] = TRUE;
    }

  /* Append views */
  for (i = 1; i <= HD_DESKTOP_VIEWS; i++)
    {
      gchar *bg_image;
      GdkPixbuf *pixbuf;
      GtkTreeIter iter;
      GtkTreePath *path;

      bg_image = g_strdup_printf ("%s/.backgrounds/background-%u.png",
                                  g_get_home_dir (),
                                  i);

      pixbuf = gdk_pixbuf_new_from_file_at_scale (bg_image, 125, 75, TRUE, &error);

      if (error)
        {
          g_warning ("Could not get background image for view %u, '%s'. %s",
                     i, bg_image, error->message);
          g_clear_error (&error);
        }

      g_free (bg_image);

      if (!pixbuf)
        {
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

      if (active_views[i - 1])
        {
          gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view),
                                     path);
        }
      else
        {
          gtk_icon_view_unselect_path (GTK_ICON_VIEW (priv->icon_view),
                                       path);
        }

      if (pixbuf)
        g_object_unref (pixbuf);
      if (path)
        gtk_tree_path_free (path);
    }

  g_signal_connect (priv->icon_view, "selection-changed",
                    G_CALLBACK (selection_changed_cb), dialog);

  /* Select at least one item and/or store the only selected item */
  selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (priv->icon_view));
  if (selected)
    {
      if (selected->next)
        {
          priv->only_selected = NULL;
          g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
        }
      else
        {
          priv->only_selected = selected->data;
        }
    }
  else
    {
      priv->only_selected = gtk_tree_path_new_first ();
      gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view),
                                 priv->only_selected);
    }
  g_list_free (selected);

  gtk_widget_show (priv->icon_view);

  pannable = hildon_pannable_area_new ();
  g_object_set (pannable,
                "mov_mode", 0,
                "vscrollbar_policy", GTK_POLICY_NEVER,
                "hscrollbar_policy", GTK_POLICY_NEVER,
/*                "size-request-policy", HILDON_SIZE_REQUEST_CHILDREN, */
                NULL);
  gtk_widget_show (pannable);
  gtk_container_add (GTK_CONTAINER (pannable),
                     priv->icon_view);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
/*                     priv->icon_view); */
                    pannable);
}

GtkWidget *
hd_activate_views_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ACTIVATE_VIEWS_DIALOG,
                         NULL);

  return window;
}

