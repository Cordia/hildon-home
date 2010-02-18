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

#include <dbus/dbus.h>

#include "hd-dbus-utils.h"

DBusGConnection *
hd_get_session_dbus_connection (void)
{
  DBusGConnection *session_dbus = NULL;
  GError *error = NULL;

  session_dbus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (error)
    {
      g_warning ("%s. Could not connect to Session D-Bus. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return session_dbus;
}

DBusGConnection *
hd_get_system_dbus_connection (void)
{
  DBusGConnection *system_dbus = NULL;
  GError *error = NULL;

  system_dbus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

  if (error)
    {
      g_warning ("%s. Could not connect to System D-Bus. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return system_dbus;
}

#define DBUS_BROWSER_SERVICE    "com.nokia.osso_browser"
#define DBUS_BROWSER_PATH       "/com/nokia/osso_browser/request"
#define DBUS_BROWSER_INSTERFACE "com.nokia.osso_browser"
#define DBUS_BROWSER_METHOD     "open_new_window"

void
hd_utils_open_link (const gchar *link)
{
  DBusConnection *conn = NULL;
  DBusMessage *msg = NULL;

  g_debug ("%s: opening %s", __FUNCTION__, link);

  conn = dbus_bus_get (DBUS_BUS_SESSION, NULL);
  if (!conn)
    {
      g_debug ("%s: Couldn't connect to session bus", __FUNCTION__);
      return;
    }

  msg = dbus_message_new_method_call (DBUS_BROWSER_SERVICE,
                                      DBUS_BROWSER_PATH,
                                      DBUS_BROWSER_SERVICE,
                                      DBUS_BROWSER_METHOD);
  if (!msg)
    {
      return;
    }

  dbus_message_set_auto_start (msg, TRUE);
  dbus_message_set_no_reply (msg, TRUE);
  dbus_message_append_args (msg, DBUS_TYPE_STRING, &link, DBUS_TYPE_INVALID);
  if (!dbus_connection_send (conn, msg, NULL))
    {
      return;
    }

  dbus_message_unref (msg);
}
