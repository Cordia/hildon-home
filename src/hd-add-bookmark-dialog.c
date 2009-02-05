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

#include <glib/gi18n.h>

#include "hd-bookmark-manager.h"

#include "hd-add-bookmark-dialog.h"

#define HD_ADD_BOOKMARK_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_ADD_BOOKMARK_DIALOG, HDAddBookmarkDialogPrivate))

struct _HDAddBookmarkDialogPrivate
{
  GtkWidget *selector;
};

G_DEFINE_TYPE (HDAddBookmarkDialog, hd_add_bookmark_dialog, HILDON_TYPE_PICKER_DIALOG);

static void
hd_add_bookmark_dialog_response (GtkDialog *dialog,
                                 gint       response_id)
{
  HDAddBookmarkDialogPrivate *priv = HD_ADD_BOOKMARK_DIALOG (dialog)->priv;

  /* Check if an item was selected */
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeIter iter;

      if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                              0,
                                              &iter))
        {
          hd_bookmark_manager_install_bookmark (hd_bookmark_manager_get (),
                                                &iter);
        }
    }
}

static void
hd_add_bookmark_dialog_class_init (HDAddBookmarkDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  dialog_class->response = hd_add_bookmark_dialog_response;

  g_type_class_add_private (klass, sizeof (HDAddBookmarkDialogPrivate));
}

static void
hd_add_bookmark_dialog_init (HDAddBookmarkDialog *dialog)
{
  HDAddBookmarkDialogPrivate *priv = HD_ADD_BOOKMARK_DIALOG_GET_PRIVATE (dialog);
  HildonTouchSelectorColumn *column;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext (GETTEXT_PACKAGE, "home_ti_select_bookmark"));

  /* The selector */
  priv->selector = g_object_ref (hildon_touch_selector_new ());

  /* A Column with icon and label */
  model = hd_bookmark_manager_get_model (hd_bookmark_manager_get ());
  column = hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                                model,
                                                NULL);
  g_object_unref (model);

  /* Add the icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "pixbuf", 3);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column),
                                 renderer,
                                 "text", 0);

  hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (dialog),
                                     HILDON_TOUCH_SELECTOR (priv->selector));
}

GtkWidget *
hd_add_bookmark_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ADD_BOOKMARK_DIALOG,
                         NULL);

  return window;
}

