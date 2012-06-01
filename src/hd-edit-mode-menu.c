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

#include <glib/gi18n.h>

#include <dbus/dbus-glib-bindings.h>

#include "hd-widgets.h"
#include "hd-applet-manager.h"
#include "hd-bookmark-widgets.h"
#include "hd-shortcut-widgets.h"
#include "hd-install-widgets-dialog.h"

#include "hd-activate-views-dialog.h"
#include "hd-change-background-dialog.h"

#include "hd-edit-mode-menu.h"

#define HD_EDIT_MODE_MENU_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_EDIT_MODE_MENU, HDEditModeMenuPrivate))

struct _HDEditModeMenuPrivate
{
  DBusGProxy      *contact_proxy;

  GtkWidget       *shortcuts_button;
  GtkWidget       *select_contacts_button;
  GtkWidget       *bookmarks_button;
  GtkWidget       *widgets_button;

  guint            current_view;
};

#define CONTACT_DBUS_NAME "com.nokia.osso_addressbook"
#define CONTACT_DBUS_PATH "/"
#define CONTACT_DBUS_ADD_SHORTCUT "add_shortcut"
#define CONTACT_DBUS_CAN_ADD_SHORTCUT "can_add_shortcut"

G_DEFINE_TYPE (HDEditModeMenu, hd_edit_mode_menu, HILDON_TYPE_APP_MENU);

static void
hd_edit_mode_menu_dispose (GObject *object)
{
  HDEditModeMenuPrivate *priv = HD_EDIT_MODE_MENU (object)->priv;

  if (priv->contact_proxy)
    {
      g_object_unref (priv->contact_proxy);
      priv->contact_proxy = NULL;
    }

  G_OBJECT_CLASS (hd_edit_mode_menu_parent_class)->dispose (object);
}

static void
can_add_shortcut_notify (DBusGProxy     *proxy,
                         DBusGProxyCall *call,
                         HDEditModeMenu *menu)
{
  HDEditModeMenuPrivate *priv = menu->priv;
  gboolean can_add_shortcut;
  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy,
                             call,
                             &error,
                             G_TYPE_BOOLEAN, &can_add_shortcut,
                             G_TYPE_INVALID))
    {
      if (can_add_shortcut)
        gtk_widget_show (priv->select_contacts_button);
      else
        gtk_widget_hide (priv->select_contacts_button);
    }
  else
    {
      g_warning ("Error calling can_add_shortcut. %s", error->message);
      g_error_free (error);
      
      gtk_widget_hide (priv->select_contacts_button);
    }  
}

static gboolean
is_widgets_model_empty (HDWidgets *widgets)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean result;

  model = hd_widgets_get_model (widgets);

  result = gtk_tree_model_get_iter_first (model, &iter);

  g_object_unref (model);

  return result;
}

static void
update_menu_button_visibility (GtkWidget *button,
                               HDWidgets *widgets)
{
  if (is_widgets_model_empty (widgets))
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

static void
hd_edit_mode_menu_show (GtkWidget *widget)
{
  HDEditModeMenuPrivate *priv = HD_EDIT_MODE_MENU (widget)->priv;

  if (gtk_grab_get_current ())
    {
      gtk_widget_hide (widget);
      return;
    }

  /* Update Add contact button */
  gtk_widget_show (priv->select_contacts_button);
  dbus_g_proxy_begin_call_with_timeout (priv->contact_proxy,
                                        CONTACT_DBUS_CAN_ADD_SHORTCUT,
                                        (DBusGProxyCallNotify) can_add_shortcut_notify,
                                        g_object_ref (widget),
                                        (GDestroyNotify) g_object_unref,
                                        5000,
                                        G_TYPE_INVALID);

  /* Update Add shortcut button */
  update_menu_button_visibility (priv->shortcuts_button,
                                 hd_shortcut_widgets_get ());

#ifdef HAVE_BOOKMARKS
  /* Update Add bookmark button */
  update_menu_button_visibility (priv->bookmarks_button,
                                 hd_bookmark_widgets_get ());
#endif

  /* Update Add widget button */
  update_menu_button_visibility (priv->widgets_button,
                                 hd_applet_manager_get ());

  GTK_WIDGET_CLASS (hd_edit_mode_menu_parent_class)->show (widget);
}

static void
hd_edit_mode_menu_class_init (HDEditModeMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = hd_edit_mode_menu_dispose;

  widget_class->show = hd_edit_mode_menu_show;

  g_type_class_add_private (klass, sizeof (HDEditModeMenuPrivate));
}

static void
select_widgets_clicked_cb (GtkButton *button,
                           HDWidgets *widgets)
{
  GtkWidget *dialog = hd_install_widgets_dialog_new (widgets);

  gtk_widget_show (dialog);
}

static void
change_background_clicked_cb (GtkButton      *button,
                              HDEditModeMenu *menu)
{
  HDEditModeMenuPrivate *priv = menu->priv;

  GtkWidget *dialog = hd_change_background_dialog_new (priv->current_view);

  gtk_widget_show (dialog);
}

static void
select_contacts_clicked_cb (GtkButton      *button,
                            HDEditModeMenu *menu)
{
  HDEditModeMenuPrivate *priv = menu->priv;

  dbus_g_proxy_call_no_reply (priv->contact_proxy,
                              CONTACT_DBUS_ADD_SHORTCUT,
                              G_TYPE_INVALID,
                              G_TYPE_INVALID);
}

static void
manage_views_clicked_cb (GtkButton      *button,
                         HDEditModeMenu *menu)
{
  GtkWidget *dialog = hd_activate_views_dialog_new ();

  gtk_widget_show (dialog);
}

static void
personalization_clicked_cb (GtkButton      *button,
                            HDEditModeMenu *menu)
{
  GError *error = NULL;

  g_spawn_command_line_async ("/usr/bin/personalisation_app", &error);

  if (error)
    {
      g_warning ("%s. Could not launch /usr/bin/personalisation_app. %s", __FUNCTION__, error->message);
      g_error_free (error);
    }
}

static void
hd_edit_mode_menu_init (HDEditModeMenu *menu)
{
  HDEditModeMenuPrivate *priv;
  GtkWidget *button;
  DBusGConnection *connection;
  GError *error = NULL;

  menu->priv = HD_EDIT_MODE_MENU_GET_PRIVATE (menu);
  priv = menu->priv;

  priv->shortcuts_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                "home_me_select_shortcuts"));
  g_signal_connect_after (priv->shortcuts_button, "clicked",
                          G_CALLBACK (select_widgets_clicked_cb), hd_shortcut_widgets_get ());
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (priv->shortcuts_button));

  priv->select_contacts_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                      "home_me_select_contacts"));
  g_signal_connect_after (priv->select_contacts_button, "clicked",
                          G_CALLBACK (select_contacts_clicked_cb), menu);
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (priv->select_contacts_button));

  priv->bookmarks_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                "home_me_select_bookmarks"));
#ifdef HAVE_BOOKMARKS
  g_signal_connect_after (priv->bookmarks_button, "clicked",
                          G_CALLBACK (select_widgets_clicked_cb), hd_bookmark_widgets_get ());
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (priv->bookmarks_button));
#endif

  priv->widgets_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                              "home_me_select_widgets"));
  g_signal_connect_after (priv->widgets_button, "clicked",
                          G_CALLBACK (select_widgets_clicked_cb), hd_applet_manager_get ());
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (priv->widgets_button));

  button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_change_background"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (change_background_clicked_cb), menu);
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_manage_views"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (manage_views_clicked_cb), menu);
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  button = gtk_button_new_with_label (dgettext ("hildon-control-panel-personalisation",
                                                "pers_ti_personalization"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (personalization_clicked_cb), menu);
  hildon_app_menu_append (HILDON_APP_MENU (menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  /* Hide on delete */
  g_signal_connect (menu, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* DBus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error != NULL)
    {
      g_warning ("Failed to open connection to session bus: %s\n",
                 error->message);
      g_error_free (error);
      return;
    }

  priv->contact_proxy = dbus_g_proxy_new_for_name (connection,
                                                   CONTACT_DBUS_NAME,
                                                   CONTACT_DBUS_PATH,
                                                   CONTACT_DBUS_NAME);

}

GtkWidget *
hd_edit_mode_menu_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_EDIT_MODE_MENU,
                         NULL);

  return window;
}

void
hd_edit_mode_menu_set_current_view (HDEditModeMenu *menu,
                                    guint           current_view)
{
  HDEditModeMenuPrivate *priv;

  g_return_if_fail (HD_IS_EDIT_MODE_MENU (menu));

  priv = menu->priv;

  priv->current_view = current_view;
}
