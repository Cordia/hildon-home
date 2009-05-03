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

#include "hd-task-manager.h"

#include "hd-add-task-dialog.h"

#define HD_ADD_TASK_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_ADD_TASK_DIALOG, HDAddTaskDialogPrivate))

struct _HDAddTaskDialogPrivate
{
  GtkWidget *selector;
};

G_DEFINE_TYPE (HDAddTaskDialog, hd_add_task_dialog, HILDON_TYPE_PICKER_DIALOG);

static void
hd_add_task_dialog_response (GtkDialog *dialog,
                             gint       response_id)
{
  HDAddTaskDialogPrivate *priv = HD_ADD_TASK_DIALOG (dialog)->priv;

  /* Check if an item was selected */
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeIter iter;

      if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                              0,
                                              &iter))
        {
          hd_task_manager_install_task (hd_task_manager_get (),
                                        &iter);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
hd_add_task_dialog_class_init (HDAddTaskDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  dialog_class->response = hd_add_task_dialog_response;

  g_type_class_add_private (klass, sizeof (HDAddTaskDialogPrivate));
}

static void
hd_add_task_dialog_init (HDAddTaskDialog *dialog)
{
  HDAddTaskDialogPrivate *priv = HD_ADD_TASK_DIALOG_GET_PRIVATE (dialog);
  GtkTreeModel *model;
  HildonTouchSelectorColumn *column;
  GtkCellRenderer *renderer;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), gettext ("home_ti_select_shortcut"));

  /* The selector */
  priv->selector = hildon_touch_selector_new ();


  /* Create empty column */
  model = hd_task_manager_get_model (hd_task_manager_get ());
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
hd_add_task_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ADD_TASK_DIALOG,
                         NULL);

  return window;
}

