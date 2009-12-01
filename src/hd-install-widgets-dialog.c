/*
 * This file is part of hildon-home
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

#include "hd-install-widgets-dialog.h"

#define HD_INSTALL_WIDGETS_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INSTALL_WIDGETS_DIALOG, HDInstallWidgetsDialogPrivate))

struct _HDInstallWidgetsDialogPrivate
{
  GtkWidget *selector;

  HDWidgets *widgets;
};

enum
{
  PROP_0,
  PROP_WIDGETS,
};

G_DEFINE_TYPE (HDInstallWidgetsDialog, hd_install_widgets_dialog, HILDON_TYPE_PICKER_DIALOG);

static void
hd_install_widgets_dialog_response (GtkDialog *dialog,
                                 gint       response_id)
{
  HDInstallWidgetsDialogPrivate *priv = HD_INSTALL_WIDGETS_DIALOG (dialog)->priv;

  /* Check if an item was selected */
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreePath *path;

      path = hildon_touch_selector_get_last_activated_row (HILDON_TOUCH_SELECTOR (priv->selector),
                                                           0);

      if (path)
        {
          hd_widgets_install_widget (priv->widgets,
                                     path);

          gtk_tree_path_free (path);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
hd_install_widgets_dialog_constructed (GObject *object)
{
  HDInstallWidgetsDialogPrivate *priv = HD_INSTALL_WIDGETS_DIALOG (object)->priv;

  if (G_OBJECT_CLASS (hd_install_widgets_dialog_parent_class)->constructed)
    G_OBJECT_CLASS (hd_install_widgets_dialog_parent_class)->constructed (object);

  GtkTreeModel *model;
  HildonTouchSelectorColumn *column;

  gtk_window_set_title (GTK_WINDOW (object),
                        hd_widgets_get_dialog_title (priv->widgets));

  priv->selector = hildon_touch_selector_new ();

  model = hd_widgets_get_model (priv->widgets);
  column = hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (priv->selector),
                                                model,
                                                NULL);

  hd_widgets_setup_column_renderers (priv->widgets,
                                     GTK_CELL_LAYOUT (column));

  g_object_set (column,
                "text-column", hd_widgets_get_text_column (priv->widgets),
                NULL);

  hildon_touch_selector_set_hildon_ui_mode (HILDON_TOUCH_SELECTOR (priv->selector),
                                            HILDON_UI_MODE_NORMAL);
  hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (object),
                                     HILDON_TOUCH_SELECTOR (priv->selector));

  g_object_unref (model);
}

static void
hd_install_widgets_dialog_get_property (GObject      *object,
                                        guint         prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_install_widgets_dialog_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HDInstallWidgetsDialogPrivate *priv = HD_INSTALL_WIDGETS_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_WIDGETS:
      priv->widgets = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_install_widgets_dialog_dispose (GObject *object)
{
  HDInstallWidgetsDialogPrivate *priv = HD_INSTALL_WIDGETS_DIALOG (object)->priv;

  if (priv->widgets)
    priv->widgets = (g_object_unref (priv->widgets), NULL);
  
  G_OBJECT_CLASS (hd_install_widgets_dialog_parent_class)->dispose (object);
}

static void
hd_install_widgets_dialog_class_init (HDInstallWidgetsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  object_class->constructed = hd_install_widgets_dialog_constructed;
  object_class->get_property = hd_install_widgets_dialog_get_property;
  object_class->set_property = hd_install_widgets_dialog_set_property;
  object_class->dispose = hd_install_widgets_dialog_dispose;

  dialog_class->response = hd_install_widgets_dialog_response;

  g_object_class_install_property (object_class,
                                   PROP_WIDGETS,
                                   g_param_spec_object ("widgets",
                                                        "Widgets",
                                                        "Widgets instance",
                                                        HD_TYPE_WIDGETS,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDInstallWidgetsDialogPrivate));
}

static void
hd_install_widgets_dialog_init (HDInstallWidgetsDialog *dialog)
{
  dialog->priv = HD_INSTALL_WIDGETS_DIALOG_GET_PRIVATE (dialog);
}

GtkWidget *
hd_install_widgets_dialog_new (HDWidgets *widgets)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_INSTALL_WIDGETS_DIALOG,
                         "widgets", widgets,
                         NULL);

  return window;
}
