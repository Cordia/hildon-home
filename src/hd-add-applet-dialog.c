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

#include "hd-applet-manager.h"

#include "hd-add-applet-dialog.h"

#define HD_ADD_APPLET_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_ADD_APPLET_DIALOG, HDAddAppletDialogPrivate))

struct _HDAddAppletDialogPrivate
{
  GtkWidget *selector;
};

G_DEFINE_TYPE (HDAddAppletDialog, hd_add_applet_dialog, HILDON_TYPE_PICKER_DIALOG);

static void
hd_add_applet_dialog_response (GtkDialog *dialog,
                               gint       response_id)
{
  HDAddAppletDialogPrivate *priv = HD_ADD_APPLET_DIALOG (dialog)->priv;

  /* Check if an item was selected */
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeIter iter;

      if (hildon_touch_selector_get_selected (HILDON_TOUCH_SELECTOR (priv->selector),
                                              0,
                                              &iter))
        {
          hd_applet_manager_install_applet (hd_applet_manager_get (),
                                            &iter);
        }
    }
}
static void
hd_add_applet_dialog_class_init (HDAddAppletDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  dialog_class->response = hd_add_applet_dialog_response;

  g_type_class_add_private (klass, sizeof (HDAddAppletDialogPrivate));
}


static void
hd_add_applet_dialog_init (HDAddAppletDialog *dialog)
{
  HDAddAppletDialogPrivate *priv = HD_ADD_APPLET_DIALOG_GET_PRIVATE (dialog);
  GtkTreeModel *model;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext (GETTEXT_PACKAGE, "home_ti_select_widgets"));

  /* Create touch selector */
  priv->selector = hildon_touch_selector_new ();

  /* Text column */
  model = hd_applet_manager_get_model (hd_applet_manager_get ());
  hildon_touch_selector_append_text_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                            model,
                                            TRUE);
  g_object_unref (model);

  hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (dialog),
                                     HILDON_TOUCH_SELECTOR (priv->selector));
}

GtkWidget *
hd_add_applet_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ADD_APPLET_DIALOG,
                         NULL);

  return window;
}

