/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
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

#include <gdk/gdkx.h>

#include <hildon/hildon.h>

#include <X11/Xlib.h>
#include <stdlib.h>

#include <libosso.h>

#include "hd-activate-views-dialog.h"
#include "hd-add-applet-dialog.h"
#include "hd-applet-manager.h"
#include "hd-add-bookmark-dialog.h"
#include "hd-bookmark-manager.h"
#include "hd-add-task-dialog.h"
#include "hd-task-manager.h"
#include "hd-change-background-dialog.h"

#include "hd-hildon-home-dbus.h"
#include "hd-hildon-home-dbus-glue.h"

#define HD_HILDON_HOME_DBUS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_HILDON_HOME_DBUS, HDHildonHomeDBusPrivate))

struct _HDHildonHomeDBusPrivate
{
  DBusGConnection *connection;
  DBusConnection  *sysbus_conn;

  DBusGProxy      *hd_home_proxy;
  DBusGProxy      *contact_proxy;

  GtkWidget       *menu;

  GtkWidget       *shortcuts_button;
  GtkWidget       *select_contacts_button;
  GtkWidget       *bookmarks_button;
  GtkWidget       *widgets_button;

  guint            current_view;
};

#define HD_HILDON_HOME_DBUS_DBUS_NAME  "com.nokia.HildonHome" 
#define HD_HILDON_HOME_DBUS_DBUS_PATH  "/com/nokia/HildonHome"

#define HD_HILDON_DESKTOP_HOME_DBUS_NAME  "com.nokia.HildonDesktop.Home" 
#define HD_HILDON_DESKTOP_HOME_DBUS_PATH  "/com/nokia/HildonDesktop/Home"

#define CONTACT_DBUS_NAME "com.nokia.osso_addressbook"
#define CONTACT_DBUS_PATH "/"
#define CONTACT_DBUS_ADD_SHORTCUT "add_shortcut"
#define CONTACT_DBUS_CAN_ADD_SHORTCUT "can_add_shortcut"

#define DSME_SIGNAL_INTERFACE "com.nokia.dsme.signal"
#define DSME_SHUTDOWN_SIGNAL_NAME "shutdown_ind"

G_DEFINE_TYPE (HDHildonHomeDBus, hd_hildon_home_dbus, G_TYPE_OBJECT);

static DBusHandlerResult
hd_hildon_home_system_bus_signal_handler (DBusConnection *conn,
                                          DBusMessage *msg, void *data)
{
  if (dbus_message_is_signal(msg, DSME_SIGNAL_INTERFACE,
                             DSME_SHUTDOWN_SIGNAL_NAME))
    {
            /*
      g_warning ("%s: " DSME_SHUTDOWN_SIGNAL_NAME " from DSME", __func__);
      */
      exit (0);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
select_applets_clicked_cb (GtkButton        *button,
                           HDHildonHomeDBus *dbus)
{
  GtkWidget *dialog = hd_add_applet_dialog_new ();

  gtk_widget_show (dialog);
}

static void
select_shortcuts_clicked_cb (GtkButton        *button,
                             HDHildonHomeDBus *dbus)
{
  GtkWidget *dialog = hd_add_task_dialog_new ();

  gtk_widget_show (dialog);
}

static void
select_bookmarks_clicked_cb (GtkButton        *button,
                             HDHildonHomeDBus *dbus)
{
  GtkWidget *dialog = hd_add_bookmark_dialog_new ();

  gtk_widget_show (dialog);
}

static void
change_background_clicked_cb (GtkButton        *button,
                              HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  GtkWidget *dialog = hd_change_background_dialog_new (priv->current_view);

  gtk_widget_show (dialog);
}

static void
select_contacts_clicked_cb (GtkButton        *button,
                            HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  dbus_g_proxy_call_no_reply (priv->contact_proxy,
                              CONTACT_DBUS_ADD_SHORTCUT,
                              G_TYPE_INVALID,
                              G_TYPE_INVALID);
}

static void
manage_views_clicked_cb (GtkButton        *button,
                         HDHildonHomeDBus *dbus)
{
  GtkWidget *dialog = hd_activate_views_dialog_new ();

  gtk_widget_show (dialog);
}

static void
personalization_clicked_cb (GtkButton        *button,
                            HDHildonHomeDBus *dbus)
{
  osso_context_t *osso;

  osso = osso_initialize ("hildon-home", "1", 0, NULL);

  osso_cp_plugin_execute (osso,
                          "libpersonalisation.so",
                          NULL,
                          TRUE);

  osso_deinitialize (osso);
}

static void
hd_hildon_home_dbus_init (HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint result;
  DBusError derror;
  GtkWidget *button;

  priv = dbus->priv = HD_HILDON_HOME_DBUS_GET_PRIVATE (dbus);

  dbus->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  dbus_error_init (&derror);
  dbus->priv->sysbus_conn = dbus_bus_get (DBUS_BUS_SYSTEM, &derror);

  if (error != NULL || dbus_error_is_set (&derror))
    {
      g_warning ("Failed to open connection to bus: %s\n",
                 error->message);

      g_error_free (error);

      return;
    }

  bus_proxy = dbus_g_proxy_new_for_name (dbus->priv->connection,
                                         DBUS_SERVICE_DBUS,
                                         DBUS_PATH_DBUS,
                                         DBUS_INTERFACE_DBUS);

  if (!org_freedesktop_DBus_request_name (bus_proxy,
                                          HD_HILDON_HOME_DBUS_DBUS_NAME,
                                          DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
                                          DBUS_NAME_FLAG_REPLACE_EXISTING |
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &result, 
                                          &error))
    {
      g_warning ("Could not register name: %s", error->message);

      g_error_free (error);

      return;
    }

  g_object_unref (bus_proxy);

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS) return;

  dbus_g_object_type_install_info (HD_TYPE_HILDON_HOME_DBUS,
                                   &dbus_glib_hd_hildon_home_dbus_object_info);

  dbus_g_connection_register_g_object (dbus->priv->connection,
                                       HD_HILDON_HOME_DBUS_DBUS_PATH,
                                       G_OBJECT (dbus));

  g_debug ("%s registered to session bus at %s", HD_HILDON_HOME_DBUS_DBUS_NAME, HD_HILDON_HOME_DBUS_DBUS_PATH);

  dbus->priv->hd_home_proxy = dbus_g_proxy_new_for_name (dbus->priv->connection,
                                                         HD_HILDON_DESKTOP_HOME_DBUS_NAME,
                                                         HD_HILDON_DESKTOP_HOME_DBUS_PATH,
                                                         HD_HILDON_DESKTOP_HOME_DBUS_NAME);
  dbus->priv->contact_proxy = dbus_g_proxy_new_for_name (dbus->priv->connection,
                                                         CONTACT_DBUS_NAME,
                                                         CONTACT_DBUS_PATH,
                                                         CONTACT_DBUS_NAME);

  /* listen to shutdown_ind from DSME */
  dbus_bus_add_match (dbus->priv->sysbus_conn, "type='signal', interface='"
                      DSME_SIGNAL_INTERFACE "'", NULL);

  dbus_connection_add_filter (dbus->priv->sysbus_conn,
                              hd_hildon_home_system_bus_signal_handler,
                              NULL, NULL);

  /* 
   * Create menu here so we have a window to listen for
   * theme changes
   */

  priv->menu = hildon_app_menu_new ();

  priv->shortcuts_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                "home_me_select_shortcuts"));
  g_signal_connect_after (priv->shortcuts_button, "clicked",
                          G_CALLBACK (select_shortcuts_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (priv->shortcuts_button));

  priv->select_contacts_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                      "home_me_select_contacts"));
  g_signal_connect_after (priv->select_contacts_button, "clicked",
                          G_CALLBACK (select_contacts_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (priv->select_contacts_button));

  priv->bookmarks_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                                "home_me_select_bookmarks"));
  g_signal_connect_after (priv->bookmarks_button, "clicked",
                          G_CALLBACK (select_bookmarks_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (priv->bookmarks_button));

  priv->widgets_button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE,
                                                              "home_me_select_widgets"));
  g_signal_connect_after (priv->widgets_button, "clicked",
                          G_CALLBACK (select_applets_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (priv->widgets_button));

  button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_change_background"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (change_background_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_manage_views"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (manage_views_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  button = gtk_button_new_with_label (dgettext ("hildon-control-panel-personalisation",
                                                "pers_ti_personalization"));
  g_signal_connect_after (button, "clicked",
                          G_CALLBACK (personalization_clicked_cb), dbus);
  hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                          GTK_BUTTON (button));
  gtk_widget_show (button);

  /* Hide on delete */
  g_signal_connect (priv->menu, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static void 
hd_hildon_home_dbus_dispose (GObject *object)
{
  HDHildonHomeDBusPrivate *priv = HD_HILDON_HOME_DBUS (object)->priv;

  if (priv->hd_home_proxy)
    {
      g_object_unref (priv->hd_home_proxy);
      priv->hd_home_proxy = NULL;
    }

  if (priv->contact_proxy)
    {
      g_object_unref (priv->contact_proxy);
      priv->contact_proxy = NULL;
    }

  if (priv->menu)
    {
      gtk_widget_destroy (priv->menu);
      priv->menu = NULL;
    }

  G_OBJECT_CLASS (hd_hildon_home_dbus_parent_class)->dispose (object);
}

static void
hd_hildon_home_dbus_class_init (HDHildonHomeDBusClass *class)
{
  GObjectClass *g_object_class = (GObjectClass *) class;

  g_object_class->dispose = hd_hildon_home_dbus_dispose;

  g_type_class_add_private (class, sizeof (HDHildonHomeDBusPrivate));
}

/* Returns the singleton HDHildonHomeDBus instance. Should not be refed un unrefed */
HDHildonHomeDBus *
hd_hildon_home_dbus_get (void)
{
  static HDHildonHomeDBus *home = NULL;

  if (G_UNLIKELY (home == NULL))
    home = g_object_new (HD_TYPE_HILDON_HOME_DBUS, NULL);

  return home;
}

static void
can_add_shortcut_notify (DBusGProxy       *proxy,
                         DBusGProxyCall   *call,
                         HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
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

void
hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus *dbus,
                                    guint             current_view)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
  HDAppletManager *applet_manager;
  HDTaskManager *task_manager;
  HDBookmarkManager *bookmark_manager;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_debug ("hd_hildon_home_dbus_show_edit_menu (current_view: %u):", current_view);
  priv->current_view = current_view;

  /* Update Add shortcut button */
  task_manager = hd_task_manager_get ();
  model = hd_task_manager_get_model (task_manager);
  if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_widget_show (priv->shortcuts_button);
  else
    gtk_widget_hide (priv->shortcuts_button);
  g_object_unref (model);

  /* Update Add contact button */
  dbus_g_proxy_begin_call_with_timeout (priv->contact_proxy,
                                        CONTACT_DBUS_CAN_ADD_SHORTCUT,
                                        (DBusGProxyCallNotify) can_add_shortcut_notify,
                                        g_object_ref (dbus),
                                        (GDestroyNotify) g_object_unref,
                                        5000,
                                        G_TYPE_INVALID);

  /* Update Add bookmark button */
  bookmark_manager = hd_bookmark_manager_get ();
  model = hd_bookmark_manager_get_model (bookmark_manager);
  if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_widget_show (priv->bookmarks_button);
  else
    gtk_widget_hide (priv->bookmarks_button);
  g_object_unref (model);

  /* Update Add widget button */
  applet_manager = hd_applet_manager_get ();
  model = hd_applet_manager_get_model (applet_manager);
  if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_widget_show (priv->widgets_button);
  else
    gtk_widget_hide (priv->widgets_button);
  g_object_unref (model);

  /* Show menu */
  gtk_widget_show (priv->menu);
}
