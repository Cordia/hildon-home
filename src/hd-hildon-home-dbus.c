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

  DBusGProxy      *hd_home_proxy;

  GtkWidget       *menu;
};

#define HD_HILDON_HOME_DBUS_DBUS_NAME  "com.nokia.HildonHome" 
#define HD_HILDON_HOME_DBUS_DBUS_PATH  "/com/nokia/HildonHome"

#define HD_HILDON_DESKTOP_HOME_DBUS_NAME  "com.nokia.HildonDesktop.Home" 
#define HD_HILDON_DESKTOP_HOME_DBUS_PATH  "/com/nokia/HildonDesktop/Home"

G_DEFINE_TYPE (HDHildonHomeDBus, hd_hildon_home_dbus, G_TYPE_OBJECT);

static void
hd_hildon_home_dbus_init (HDHildonHomeDBus *dbus)
{
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint result;

  dbus->priv = HD_HILDON_HOME_DBUS_GET_PRIVATE (dbus);

  dbus->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (error != NULL)
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


HDHildonHomeDBus *
hd_hildon_home_dbus_get (void)
{
  static HDHildonHomeDBus *home = NULL;

  if (G_UNLIKELY (home == NULL))
    {
      home = g_object_new (HD_TYPE_HILDON_HOME_DBUS, NULL);
    }

  return home;
}

static void
select_applets_clicked_cb (GtkButton        *button,
                           HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
  GtkWidget *dialog = hd_add_applet_dialog_new ();

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);


  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

static void
select_shortcuts_clicked_cb (GtkButton        *button,
                             HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
  GtkWidget *dialog = hd_add_task_dialog_new ();

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

static void
select_bookmarks_clicked_cb (GtkButton        *button,
                             HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
  GtkWidget *dialog = hd_add_bookmark_dialog_new ();

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

static void
change_background_clicked_cb (GtkButton        *button,
                              HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;
  GtkWidget *dialog = hd_change_background_dialog_new (1);
  GtkResponseType response;

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);

  do
    {
      response = gtk_dialog_run (GTK_DIALOG (dialog));
    }
  while (response >= 0);

  gtk_widget_destroy (dialog);

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

static void
select_contacts_clicked_cb (GtkButton        *button,
                            HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);

  g_debug ("select_contacts_clicked_cb: NOT IMPLEMENTED YET");

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

static void
manage_views_clicked_cb (GtkButton        *button,
                         HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  g_debug ("manage_views_clicked_cb");

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "ShowActivateViewsDialog",
                              G_TYPE_INVALID);
}

static void
model_row_inserted_cb (GtkTreeModel *model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       GtkWidget    *button)
{
  gtk_widget_show (button);
}

static void
model_row_deleted_cb (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkWidget    *button)
{
  GtkTreeIter iter;

  /* Check if model is empty */
  if (!gtk_tree_model_get_iter_first (model, &iter))
    gtk_widget_hide (button);
}

static void
menu_hide_cb (GtkWidget        *widget,
              HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "GrabPointer",
                              G_TYPE_INVALID);
}

void
hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus *dbus,
                                    guint             current_view)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  g_debug ("hd_hildon_home_dbus_show_edit_menu (current_view: %u):", current_view);

  /* Constrcut menu */
  if (!priv->menu)
    {
      GtkWidget *button;
      Display *display;
      Window root;
      HDAppletManager *applet_manager;
      HDTaskManager *task_manager;
      HDBookmarkManager *bookmark_manager;
      GtkTreeModel *model;
      GtkTreeIter iter;

      priv->menu = hildon_app_menu_new ();
      g_signal_connect (priv->menu, "hide",
                        G_CALLBACK (menu_hide_cb), dbus);

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_select_applets"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (select_applets_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      applet_manager = hd_applet_manager_get ();
      model = hd_applet_manager_get_model (applet_manager);
      g_signal_connect (G_OBJECT (model), "row-inserted",
                        G_CALLBACK (model_row_inserted_cb), button);
      g_signal_connect (G_OBJECT (model), "row-deleted",
                        G_CALLBACK (model_row_deleted_cb), button);
      if (!gtk_tree_model_get_iter_first (model, &iter))
        gtk_widget_hide (button);
      g_object_unref (model);

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_select_shortcuts"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (select_shortcuts_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      task_manager = hd_task_manager_get ();
      model = hd_task_manager_get_model (task_manager);
      g_signal_connect (G_OBJECT (model), "row-inserted",
                        G_CALLBACK (model_row_inserted_cb), button);
      g_signal_connect (G_OBJECT (model), "row-deleted",
                        G_CALLBACK (model_row_deleted_cb), button);
      if (!gtk_tree_model_get_iter_first (model, &iter))
        gtk_widget_hide (button);
      g_object_unref (model);

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_select_bookmarks"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (select_bookmarks_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      bookmark_manager = hd_bookmark_manager_get ();
      model = hd_bookmark_manager_get_model (bookmark_manager);
      g_signal_connect (G_OBJECT (model), "row-inserted",
                        G_CALLBACK (model_row_inserted_cb), button);
      g_signal_connect (G_OBJECT (model), "row-deleted",
                        G_CALLBACK (model_row_deleted_cb), button);
      if (!gtk_tree_model_get_iter_first (model, &iter))
        gtk_widget_hide (button);
      g_object_unref (model);

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_change_background"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (change_background_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_select_contacts"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (select_contacts_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      button = gtk_button_new_with_label (dgettext (GETTEXT_PACKAGE, "home_me_manage_views"));
      g_signal_connect_after (button, "clicked",
                              G_CALLBACK (manage_views_clicked_cb), dbus);
      hildon_app_menu_append (HILDON_APP_MENU (priv->menu),
                              GTK_BUTTON (button));

      /* Set menu transient for root window */
      gtk_widget_realize (priv->menu);
      display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (priv->menu));
      root = RootWindow (display, GDK_SCREEN_XNUMBER (gtk_widget_get_screen (priv->menu)));
      XSetTransientForHint (display, GDK_WINDOW_XID (priv->menu->window), root);
    }

  /* Ungrab pointer in home */
  dbus_g_proxy_call_no_reply (priv->hd_home_proxy,
                              "UngrabPointer",
                              G_TYPE_INVALID);


  /* Show menu */
  gtk_widget_show (priv->menu);
}
