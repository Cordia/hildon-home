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

#include "hd-dbus-utils.h"

DBusGConnection *
hd_get_session_dbus_connection (void)
{
  DBusGConnection *session_dbus;
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
  DBusGConnection *system_dbus;
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
