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

#include "hd-backgrounds.h"
#include "hd-edit-mode-menu.h"

#include "hd-hildon-home-dbus.h"
#include "hd-hildon-home-dbus-glue.h"

#define HD_HILDON_HOME_DBUS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_HILDON_HOME_DBUS, HDHildonHomeDBusPrivate))

struct _HDHildonHomeDBusPrivate
{
  DBusGConnection *connection;
  DBusConnection  *sysbus_conn;

  GtkWidget       *menu;

  DBusGProxy      *hd_home_proxy;
};

#define HD_HILDON_HOME_DBUS_DBUS_NAME  "com.nokia.HildonHome" 
#define HD_HILDON_HOME_DBUS_DBUS_PATH  "/com/nokia/HildonHome"

#define HD_HILDON_DESKTOP_HOME_DBUS_NAME  "com.nokia.HildonDesktop.Home" 
#define HD_HILDON_DESKTOP_HOME_DBUS_PATH  "/com/nokia/HildonDesktop/Home"

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
hd_hildon_home_dbus_init (HDHildonHomeDBus *dbus)
{
  HDHildonHomeDBusPrivate *priv;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint result;
  DBusError derror;

  priv = dbus->priv = HD_HILDON_HOME_DBUS_GET_PRIVATE (dbus);

  dbus->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (error != NULL)
    {
      g_warning ("Failed to open connection to session bus: %s\n",
                 error->message);
      g_error_free (error);
      return;
    }

  dbus_error_init (&derror);
  dbus->priv->sysbus_conn = dbus_bus_get (DBUS_BUS_SYSTEM, &derror);

  if (dbus_error_is_set (&derror))
    {
      g_warning ("Failed to open connection to system bus: %s\n",
                 derror.message);
      dbus_error_free (&derror);
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

  priv->menu = hd_edit_mode_menu_new ();
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

/* Returns the singleton HDHildonHomeDBus instance. Should not be refed un unrefed */
HDHildonHomeDBus *
hd_hildon_home_dbus_get (void)
{
  static HDHildonHomeDBus *home = NULL;

  if (G_UNLIKELY (home == NULL))
    home = g_object_new (HD_TYPE_HILDON_HOME_DBUS, NULL);

  return home;
}

void
hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus      *dbus,
                                    guint                  current_view,
                                    DBusGMethodInvocation *context)
{
  HDHildonHomeDBusPrivate *priv = dbus->priv;

  hd_edit_mode_menu_set_current_view (HD_EDIT_MODE_MENU (priv->menu),
                                      current_view);
  gtk_widget_show (priv->menu);

  dbus_g_method_return (context);
}

void
hd_hildon_home_dbus_set_background_image (HDHildonHomeDBus      *dbus,
                                          const char            *uri,
                                          DBusGMethodInvocation *context)
{
  hd_backgrounds_set_current_background (hd_backgrounds_get (),
                                         uri);
  dbus_g_method_return (context);
}
